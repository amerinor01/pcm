#!/usr/bin/env python3
"""
 PCMC Code Generator

Generates algorithm-specific PCMC and header files from 
a YAML configuration file and a fabric constants YAML.
"""

import yaml
import argparse
import os
import math
from typing import List, Dict, Any


# Custom YAML loader to support !calc tag as plain expression string
class CalcLoader(yaml.SafeLoader):
    pass

def construct_calc(loader: yaml.Loader, node: yaml.nodes.ScalarNode) -> str:
    # Treat the scalar tagged with "!eval" as a raw expression string
    return node.value

CalcLoader.add_constructor('!eval', construct_calc)


class AlgorithmCodeGenerator:

    def __init__(self, constants_file: str, config_file: str):
        with open(constants_file, 'r') as f:
            self.fabric_constants: Dict[str, Any] = yaml.safe_load(f)
        
        # Load algorithm config, allowing !calc tags
        with open(config_file, 'r') as f:
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
        pcmc_path = os.path.join(output_dir, f"{self.algorithm_name}_pcmc.c")
        with open(pcmc_path, "w") as f:
            f.write(pcmc_content)
        print(f"Generated {pcmc_path}")

    def _assign_indices_to_names(self) -> None:
        """Enumerate all names inside each cathegory (signals/controls/variables)."""
        for idx, signal in enumerate(self.config.get("signals", [])):
            signal["index"] = idx
        for idx, control in enumerate(self.config.get("controls", [])):
            control["index"] = idx
        for idx, var in enumerate(self.config.get("variables", [])):
            var["index"] = idx

    def _compute_all_values(self) -> None:
        """Compute constants, thresholds, and initial values from expressions or refs."""
        # build initial context with fabric constants
        ctx: Dict[str, Any] = dict(self.fabric_constants)
        # include computed constants first
        # constants may depend on each other
        consts = self.config.get('constants', [])
        unresolved = consts[:]
        while unresolved:
            progress = False
            for c in unresolved[:]:
                name = c['name']
                raw = c.get('value')
                try:
                    if isinstance(raw, str) and raw in ctx:
                        val = ctx[raw]
                    elif isinstance(raw, (int, float)):
                        val = raw
                    elif isinstance(raw, str):
                        val = eval(raw, {**vars(math), 'sqrt': math.sqrt}, ctx)
                    else:
                        val = raw
                except NameError:
                    continue
                c['value'] = val
                ctx[name] = val
                unresolved.remove(c)
                progress = True
            if not progress:
                missing = [c['name'] for c in unresolved]
                raise ValueError(f"Unresolved constants: {missing}")
        # helper to evaluate any field
        def resolve(raw):
            if isinstance(raw, str) and raw in ctx:
                return ctx[raw]
            if isinstance(raw, (int, float)):
                return raw
            if isinstance(raw, str):
                return eval(raw, {**vars(math), 'sqrt': math.sqrt}, ctx)
            return raw
        # compute other fields
        for sig in self.config.get('signals', []):
            if 'trigger_threshold' in sig:
                sig['trigger_threshold'] = resolve(sig['trigger_threshold'])
        for ctrl in self.config.get('controls', []):
            if 'initial_value' in ctrl:
                ctrl['initial_value'] = resolve(ctrl['initial_value'])
        for var in self.config.get('variables', []):
            if 'initial_value' in var:
                var['initial_value'] = resolve(var['initial_value'])

    def _generate_pcmc(self) -> str:
        lines: List[str] = []
        lines.append('#include "algo_utils.h"')
        lines.append('#include "pcm.h"')
        lines.append(f'#include "{self.algorithm_name}.h"')
        lines.append('')
        lines.append(f'int __pcmc_init(pcm_handle_t new_handle)')
        lines.append('{')
        lines.append('    /* Signals */')
        for sig in self.config.get('signals', []):
            typ, accum = sig['type'], sig['accumulation']
            name = f"SIG_{sig['name']}"
            lines.append(f'    PCMC_EXIT_ON_ERR(register_signal_pcmc({typ}, {accum}, {name}, new_handle), PCM_SUCCESS);')
            if 'trigger_threshold' in sig:
                thr = sig['trigger_threshold']
                lines.append(f'    PCMC_EXIT_ON_ERR(register_signal_invoke_trigger_pcmc({name}, {thr}, new_handle), PCM_SUCCESS);')
        lines.append('')
        lines.append('    /* Controls */')
        for ctrl in self.config.get('controls', []):
            typ = ctrl['type']
            name = f"CTRL_{ctrl['name']}"
            init = ctrl.get('initial_value')
            lines.append(f'    PCMC_EXIT_ON_ERR(register_control_pcmc({typ}, {name}, new_handle), PCM_SUCCESS);')
            if init is not None:
                lines.append(f'    PCMC_EXIT_ON_ERR(register_control_initial_value_pcmc({name}, {init}, new_handle), PCM_SUCCESS);')
        lines.append('')
        lines.append('    /* Variables */')
        for var in self.config.get('variables', []):
            name = f"VAR_{var['name']}"
            init = var.get('initial_value')
            lines.append(f'    PCMC_EXIT_ON_ERR(register_local_state_pcmc({name}, new_handle), PCM_SUCCESS);')
            if init is not None:
                lines.append(f'    PCMC_EXIT_ON_ERR(register_local_state_initial_value_pcmc({name}, {init}, new_handle), PCM_SUCCESS);')
        lines.append('')
        lines.append('    return PCM_SUCCESS;')
        lines.append('}')
        return '\n'.join(lines)

    def _generate_header(self) -> str:
        """Generate header file for algorithm and PCMC"""
        lines: List[str] = []

        # Guard and include
        lines.append(f"#ifndef _{self.algorithm_name_upper}_H_")
        lines.append(f"#define _{self.algorithm_name_upper}_H_")
        lines.append("")
        lines.append('#include "pcm.h"')
        lines.append("")

        # Emit constants as #define
        for const in self.config.get("constants", []):
            val = const["value"]
            lines.append(f"#define {const['name'].upper()} {val}")
        lines.append("")

        def __generate_enum(block: str, items: List[Dict[str, Any]], cathegory: str) -> None:
            """Helper to define enum for a given cathegory."""
            lines.append(f"enum {self.algorithm_name}_{block} {{")
            for item in items:
                lines.append(f"    {cathegory}_{item['name']} = {item['index']},")
            lines.append("};")
            lines.append("")

        __generate_enum("signals", self.config.get("signals", []), "SIG")
        __generate_enum("controls", self.config.get("controls", []), "CTRL")
        __generate_enum("variables", self.config.get("variables", []), "VAR")

        # Function declarations
        for decl in [
            "#ifdef HANDLER_BUILD",
            "int algorithm_main();",
            "#else",
            "",
            "#ifdef __cplusplus",
            'extern "C" {',
            "#endif",
            "",
            f"int __pcmc_init(handle_t new_handle);",
            "",
            "#ifdef __cplusplus",
            "}",
            "#endif",
            "",
            "#endif",
            f"#endif /* _{self.algorithm_name_upper}_H_ */"
        ]:
            lines.append(decl)

        return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Generate algorithm files from YAML config"
    )
    parser.add_argument(
        "constants", help="YAML fabric constants file"
    )
    parser.add_argument(
        "config", help="YAML algorithm configuration file"
    )
    parser.add_argument(
        "-o", "--output", default=".", help="Output directory"
    )

    args = parser.parse_args()

    generator = AlgorithmCodeGenerator(args.constants, args.config)
    generator.run(args.output)


if __name__ == "__main__":
    main()
