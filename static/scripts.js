var autotare_service_uuid = '75c3c6d0-75e4-4223-a823-bdc65e738996'
var autotare_weight_uuid = '722bc7b5-1728-4e39-8867-3161d8dd5e20'
var ser3 = '6e400003-b5a3-f393-e0a9-e50e24dcca9e'
var ser4 = '6e400001-b5a3-f393-e0a9-e50e24dcca9e'

var count = 0
function handle_weight(ev) {
  var buf = ev.target.value;
  var a = buf.getUint8(0);
  var b = buf.getUint8(1);
  var c = buf.getUint8(2);
  var d = buf.getUint8(3);
  var val = a | (b << 8) | (c << 16) | (d << 24)
  /* i think javascript promises to do bit operations 32 bit signed */
  $("#rawval").html(val);
}

function do_button1 () {
  navigator.bluetooth.requestDevice({
    filters: [{
      name: 'autotare'
    }],
    optionalServices: [autotare_service_uuid]
  }).then(device => {
    console.log(device);
    console.log("connected to", device.name);
    return device.gatt.connect();
  }).then(server => {
    console.log ("server", server);
    return server.getPrimaryService(autotare_service_uuid);
  }).then(service => {
    console.log("service", service);
    return service.getCharacteristic(autotare_weight_uuid);
  }).then(characteristic => {
    console.log("characteristic", characteristic);
    return characteristic.startNotifications();
  }).then(characteristic => {
    characteristic.addEventListener('characteristicvaluechanged',
				    handle_weight);
    console.log("notifications started")
  }).catch(error => {
    console.log(error);
  });
}


$(function () {
  $("#button1").click(do_button1)
});
