import { bleMonitorService } from './core/ble/ble-monitor-service';
App({onLaunch(){bleMonitorService().initialize()},onHide(){bleMonitorService().stopScan()}});
