import yaml
import os
import math
from typing import List, Dict, Any


# Custom YAML loader to support !calc tag as plain expression string
class CalcLoader(yaml.SafeLoader):
    pass


def construct_calc(loader: yaml.Loader, node: yaml.nodes.ScalarNode) -> str:
    # Treat the scalar tagged with "!eval" as a raw expression string
    return node.value


CalcLoader.add_constructor("!eval", construct_calc)


class AlgorithmCodeGenerator:

    def __init__(self, fabric_constants_file: str, config_file: str):
        with open(fabric_constants_file, "r") as f:
            self.fabric_constants: Dict[str, Any] = yaml.safe_load(f)

        # Load algorithm config, allowing !calc tags
        with open(config_file, "r") as f:
            # Use custom loader so !calc yields expression strings
            self.config: Dict[str, Any] = yaml.load(f, Loader=CalcLoader)

        self.algorithm_name: str = self.config["algorithm"]["name"]
        self.algorithm_name_upper: str = self.algorithm_name.upper()

        self._assign_indices_to_names()
        self._compute_all_values()

    def run(self, output_dir: str = ".") -> None:
        """Generate header file in the specified output directory."""
        header_content = self._generate_header()
        header_path = os.path.join(output_dir, f"{self.algorithm_name}.h")
        with open(header_path, "w") as f:
            f.write(header_content)
        print(f"Generated {header_path}")

        """Generate PCMC file in the specified output directory."""
        pcmc_content = self._generate_pcmc()
        pcmc_path = os.path.join(output_dir, f"{self.algorithm_name}_spec.cpp")
        with open(pcmc_path, "w") as f:
            f.write(pcmc_content)
        print(f"Generated {pcmc_path}")

    def _assign_indices_to_names(self) -> None:
        """Enumerate all names inside each cathegory (signals/controls/variables)."""
        for idx, signal in enumerate(self.config.get("signals", [])):
            signal["index"] = idx
        for idx, control in enumerate(self.config.get("controls", [])):
            control["index"] = idx
        idx_offset = 0
        for idx, var in enumerate(self.config.get("variables", [])):
            var["index"] = idx_offset
            var["num_entries"] = 1
            if "arr_len" in var:
                var["num_entries"] = int(var["arr_len"])
            idx_offset = idx_offset + var["num_entries"]

    def _compute_all_values(self) -> None:
        """Compute constants, thresholds, and initial values from expressions or refs."""
        # build initial context with fabric constants
        ctx: Dict[str, Any] = dict(self.fabric_constants)
        # include computed constants first
        # constants may depend on each other
        consts = self.config.get("constants", [])
        unresolved = consts[:]
        while unresolved:
            progress = False
            for c in unresolved[:]:
                name = c["name"]
                raw = c.get("value")
                try:
                    if isinstance(raw, str) and raw in ctx:
                        val = ctx[raw]
                    elif isinstance(raw, (int, float)):
                        val = raw
                    elif isinstance(raw, str):
                        val = eval(raw, {**vars(math), "sqrt": math.sqrt}, ctx)
                    else:
                        val = raw
                except NameError:
                    continue
                c["value"] = val
                ctx[name] = val
                unresolved.remove(c)
                progress = True
            if not progress:
                missing = [c["name"] for c in unresolved]
                raise ValueError(f"Unresolved constants: {missing}")

        # helper to evaluate any field
        def resolve(raw):
            if isinstance(raw, str) and raw in ctx:
                return ctx[raw]
            if isinstance(raw, (int, float)):
                return raw
            if isinstance(raw, str):
                return eval(raw, {**vars(math), "sqrt": math.sqrt}, ctx)
            return raw

        # compute other fields
        for sig in self.config.get("signals", []):
            if "trigger_threshold" in sig:
                sig["trigger_threshold"] = resolve(sig["trigger_threshold"])
        for ctrl in self.config.get("controls", []):
            if "initial_value" in ctrl:
                ctrl["initial_value"] = resolve(ctrl["initial_value"])
        for var in self.config.get("variables", []):
            if "initial_value" in var:
                var["initial_value"] = resolve(var["initial_value"])

        # Snapshot layout:
        # mask | signals | controls | variables
        self.mask_offset = 0
        self.set_signal_mask_offset = self.mask_offset + 1
        self.signal_offset = self.set_signal_mask_offset + 1
        self.num_signals = len(self.config.get("signals", []))
        self.num_controls = len(self.config.get("controls", []))
        # Count total variable slots (including array elements)
        self.num_variable_slots = sum(
            var.get("num_entries", 1) for var in self.config.get("variables", [])
        )
        self.control_offset = self.signal_offset + self.num_signals
        self.variable_offset = self.control_offset + self.num_controls
        self.snapshot_size = (
            2
            + self.num_signals
            + self.num_controls
            + self.num_variable_slots
        )

    def _generate_pcmc(self) -> str:
        # Collect all tuple elements first
        variable_defs = []
        tuple_elements = []

        # Add signals
        if self.config.get("signals", []):
            tuple_elements.append("    /* Signals */")
            for sig in self.config.get("signals", []):
                typ, accum = sig["type"], sig["accumulation"]
                name = f"SIG_{sig['name']}"
                thr = "PCM_SIG_NO_TRIGGER"
                if "trigger_threshold" in sig:
                    thr = sig["trigger_threshold"]
                tuple_elements.append(
                    f"    pcm_vm::SignalDesc<{typ}, {accum}, {thr}, {name}>"
                )

        # Add controls
        if self.config.get("controls", []):
            tuple_elements.append("    /* Controls */")
            for ctrl in self.config.get("controls", []):
                typ = ctrl["type"]
                name = f"CTRL_{ctrl['name']}"
                init = ctrl.get("initial_value")
                tuple_elements.append(f"    pcm_vm::ControlDesc<{typ}, {init}, {name}>")

        # Add variables
        if self.config.get("variables", []):
            tuple_elements.append("    /* Variables */")
            for var in self.config.get("variables", []):
                name = f"VAR_{var['name']}"
                init = var.get("initial_value")
                dtype = var.get("type")
                variable_defs.append(
                    f"inline constexpr pcm_{dtype} init_val_{name} = {init};"
                )
                for elem_idx in range(var.get("num_entries")):
                    tuple_elements.append(
                        f"    pcm_vm::VariableDesc<pcm_{dtype}, init_val_{name}, {name} + {elem_idx}>"
                    )

        lines: List[str] = []
        lines.append("#include <tuple>")
        lines.append('#include "pcm_vm.hpp"')
        lines.append(f'#include "{self.algorithm_name}.h"')
        lines.append("")
        for var_def in variable_defs:
            lines.append(var_def)
        lines.append("using DatapathSpec = std::tuple<")
        # Add commas to all but the last non-comment element
        for i, element in enumerate(tuple_elements):
            if element.strip().startswith("/*") or element == "":
                lines.append(element)
            else:
                # Check if this is the last non-comment element
                is_last = True
                for j in range(i + 1, len(tuple_elements)):
                    if (
                        not tuple_elements[j].strip().startswith("/*")
                        and tuple_elements[j] != ""
                    ):
                        is_last = False
                        break

                if is_last:
                    lines.append(element)
                else:
                    lines.append(element + ",")

        lines.append(">;")
        lines.append(
            f'inline constexpr const char {self.algorithm_name}_AlgoName[] = "{self.algorithm_name}";'
        )
        lines.append("")
        lines.append(
            f"using {self.algorithm_name}_SnapshotLayout = pcm_vm::SnapshotMemoryLayout<"
        )
        lines.append(f"    {self.mask_offset},        // kTriggerMaskOffset")
        lines.append(f"    {self.set_signal_mask_offset}, // kSignalSetMaskOffset")
        lines.append(f"    {self.signal_offset},      // kSignalOffset")
        lines.append(f"    {self.control_offset},     // kControlOffset")
        lines.append(f"    {self.variable_offset},    // kVariableOffset")
        lines.append(f"    {self.snapshot_size}            // kSnapshotSize")
        lines.append(">;")
        lines.append("")
        lines.append(
            f"using PcmHandlerVmSpecType = pcm_vm::SimplePcmHandlerVm<{self.algorithm_name}_AlgoName, {self.algorithm_name}_SnapshotLayout, DatapathSpec>;"
        )
        lines.append(f"pcm_vm::PcmHandlerVmDesc* __{self.algorithm_name}_spec_get()")
        lines.append("{")
        lines.append("    return new PcmHandlerVmSpecType{};")
        lines.append("}")
        return "\n".join(lines)

    def _generate_header(self) -> str:
        """Generate header file for algorithm and PCMC"""
        lines: List[str] = []

        # Guard and include
        lines.append(f"#ifndef _{self.algorithm_name_upper}_H_")
        lines.append(f"#define _{self.algorithm_name_upper}_H_")
        lines.append("")
        lines.append('#include "pcm.h"')
        lines.append("")

        # Function declarations
        for decl in [
            "#ifdef HANDLER_BUILD",
            "int algorithm_main();",
            "#else",
            "",
            '#include "pcm_vm.hpp"',
            f'extern "C" pcm_vm::PcmHandlerVmDesc* __{self.algorithm_name}_spec_get();',
            "",
            "#endif",
        ]:
            lines.append(decl)

        # Emit constants as #define
        for const in self.config.get("constants", []):
            val = const["value"]
            lines.append(f"#define {const['name'].upper()} {val}")
        lines.append("")

        def __generate_enum(
            block: str, items: List[Dict[str, Any]], cathegory: str, mask: bool = False
        ) -> None:
            """Helper to define enum for a given cathegory."""
            lines.append(f"enum {self.algorithm_name}_{block} {{")
            for item in items:
                index = str(item["index"])
                if mask:
                    index = f"1 << {item['index']}"
                lines.append(f"    {cathegory}_{item['name']} = {index},")
            lines.append("};")
            lines.append("")

        for offset in [
            f"#define MASK_OFFSET {self.mask_offset}",
            f"#define SET_SIGNAL_MASK_OFFSET {self.set_signal_mask_offset}",
            f"#define SIGNAL_OFFSET {self.signal_offset}",
            f"#define CONTROL_OFFSET {self.control_offset}",
            f"#define VAR_OFFSET {self.variable_offset}",
        ]:
            lines.append(offset)
        lines.append("")

        __generate_enum("signals", self.config.get("signals", []), "SIG", mask=True)
        __generate_enum("controls", self.config.get("controls", []), "CTRL")
        __generate_enum("variables", self.config.get("variables", []), "VAR")

        lines.append("")
        lines.append(f"#endif /* _{self.algorithm_name_upper}_H_ */")

        return "\n".join(lines)
