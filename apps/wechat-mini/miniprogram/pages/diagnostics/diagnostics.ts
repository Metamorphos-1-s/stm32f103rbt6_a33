import { DeviceStore } from '../../core/store/device-store';
Page({data:{state:'CLOSED',deviceName:'-',weight:'-',diagnostics:{sequenceGaps:0,duplicates:0,crcErrors:0,resyncBytes:0}},onLoad(){const store=new DeviceStore();this.setData({state:store.state,diagnostics:store.diagnostics})},onHide(){},onUnload(){}});
