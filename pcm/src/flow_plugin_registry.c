#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "impl.h"
#include "slist.h"
#include "util.h"

// Global plugin registry
static struct slist plugin_registry = {NULL, NULL};

int flow_plugin_register(const char *name,
                         int (*init_fn)(struct flow_plugin_ops *)) {
    if (!name || !init_fn) {
        PCM_LOG_CRIT("Invalid plugin registration parameters");
        return PCM_ERROR;
    }

    // Check if plugin is already registered
    struct slist_entry *item, *prev;
    slist_foreach(&plugin_registry, item, prev) {
        (void)prev; /* suppress compiler warning */
        struct flow_plugin_registry_entry *entry =
            container_of(item, struct flow_plugin_registry_entry, list_entry);
        if (strcmp(entry->name, name) == 0) {
            PCM_LOG_CRIT("Plugin %s is already registered", name);
            return PCM_SUCCESS;
        }
    }

    struct flow_plugin_registry_entry *entry =
        calloc(1, sizeof(struct flow_plugin_registry_entry));
    if (!entry) {
        PCM_LOG_CRIT("Failed to allocate memory for plugin registry entry");
        return PCM_ERROR;
    }

    entry->name = strdup(name);
    entry->init_fn = init_fn;
    slist_insert_tail(&entry->list_entry, &plugin_registry);

    PCM_LOG_INFO("Registered flow plugin: %s", name);
    return PCM_SUCCESS;
}

int flow_plugin_ops_get(const char *name, struct flow_plugin_ops *ops) {
    if (!name || !ops) {
        PCM_LOG_CRIT("Invalid plugin lookup parameters");
        return PCM_ERROR;
    }

    struct slist_entry *item, *prev;
    slist_foreach(&plugin_registry, item, prev) {
        (void)prev; /* suppress compiler warning */
        struct flow_plugin_registry_entry *entry =
            container_of(item, struct flow_plugin_registry_entry, list_entry);
        if (strcmp(entry->name, name) == 0) {
            return entry->init_fn(ops);
        }
    }

    PCM_LOG_CRIT("Flow plugin not found: %s", name);
    return PCM_ERROR;
}

int flow_plugin_deregister(const char *name) {
    if (!name) {
        PCM_LOG_CRIT("Invalid plugin deregistration parameters");
        return PCM_ERROR;
    }

    struct slist_entry *item, *prev;
    slist_foreach(&plugin_registry, item, prev) {
        struct flow_plugin_registry_entry *entry =
            container_of(item, struct flow_plugin_registry_entry, list_entry);
        if (strcmp(entry->name, name) == 0) {
            // Remove from list
            slist_remove(&plugin_registry, item, prev);

            // Free memory
            free(entry->name);
            free(entry);

            PCM_LOG_INFO("Deregistered flow plugin: %s", name);
            return PCM_SUCCESS;
        }
    }

    PCM_LOG_CRIT("Plugin %s not found for deregistration", name);
    return PCM_ERROR;
}

void flow_plugin_cleanup(void) {
    if (!slist_empty(&plugin_registry)) {
        PCM_LOG_CRIT("Flow plugin registry list is not empty");
    }
}

// Destructor function to clean up all plugins when PCM library is unloaded
__attribute__((destructor)) void pcm_library_cleanup(void) {
    flow_plugin_cleanup();
}