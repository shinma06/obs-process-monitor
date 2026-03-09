#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QMainWindow>

#include "ProcessMonitorWidget.hpp"
#include "plugin-macros.generated.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
    return "Displays OBS process CPU and RAM usage as a dockable panel.";
}

// Keep a reference so we can clean up on unload
static ProcessMonitorWidget *g_monitorWidget = nullptr;

bool obs_module_load(void)
{
    obs_frontend_push_ui_translation(obs_module_get_string);

    auto *mainWindow =
        static_cast<QMainWindow *>(obs_frontend_get_main_window());

    g_monitorWidget = new ProcessMonitorWidget(mainWindow);

    // obs_frontend_add_dock_by_id: added in OBS 30.0
    // Creates a QDockWidget wrapper and adds it to the Docks menu.
    obs_frontend_add_dock_by_id(
        "obs-process-monitor",   // unique id
        "Process Monitor",       // dock title (shown in Docks menu)
        g_monitorWidget          // QWidget* content
    );

    obs_frontend_pop_ui_translation();

    obs_log(LOG_INFO,
            "plugin loaded successfully (version %s)",
            PLUGIN_VERSION);
    return true;
}

void obs_module_unload(void)
{
    // Remove the dock from OBS UI
    obs_frontend_remove_dock("obs-process-monitor");

    // g_monitorWidget is owned by the dock's QDockWidget (Qt parent),
    // so no manual delete is needed here.
    g_monitorWidget = nullptr;

    obs_log(LOG_INFO, "plugin unloaded");
}
