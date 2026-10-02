#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QMainWindow>
#include <QPointer>

#include "ProcessMonitorWidget.hpp"
#include "plugin-macros.generated.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
    return "Displays Windows resource usage as a dockable panel.";
}

static QPointer<ProcessMonitorWidget> g_monitorWidget;

bool obs_module_load(void)
{
    auto *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
    if (!mainWindow) {
        blog(LOG_ERROR, "[%s] OBS main window is unavailable", PLUGIN_NAME);
        return false;
    }

    obs_frontend_push_ui_translation(obs_module_get_string);
    auto *widget = new ProcessMonitorWidget(mainWindow);
    const bool added = obs_frontend_add_dock_by_id(PLUGIN_NAME, "Process Monitor", widget);
    obs_frontend_pop_ui_translation();
    if (!added) {
        delete widget;
        blog(LOG_ERROR, "[%s] Could not register dock", PLUGIN_NAME);
        return false;
    }
    g_monitorWidget = widget;
    blog(LOG_INFO, "[%s] loaded version %s (source %s)", PLUGIN_NAME, PLUGIN_VERSION, PLUGIN_SOURCE_SHA);
    return true;
}

void obs_module_unload(void)
{
    if (g_monitorWidget) {
        // Destroy plugin-owned content while its code is still loaded. QPointer also
        // covers OBS having already destroyed the dock during application shutdown.
        delete g_monitorWidget.data();
        obs_frontend_remove_dock(PLUGIN_NAME);
    }
    blog(LOG_INFO, "[%s] unloaded", PLUGIN_NAME);
}
