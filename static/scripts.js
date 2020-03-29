var xautotare_service_uuid = '75c3c6d0-75e4-4223-a823-bdc65e738996'
var autotare_service_uuid = '0000ec00-0000-1000-8000-00805f9b34fb'
var autotare_weight_uuid = '722bc7b5-1728-4e39-8867-3161d8dd5e20'

var cal1_raw = -9550;
var cal1_g = 0;

var cal2_raw = -16210;
var cal2_g = 10;

function raw_to_grams (raw) {
  return ((raw - cal1_raw) / (cal2_raw - cal1_raw) * (cal2_g - cal1_g) 
	  + cal1_g);
}

function lpf_step (oldval, newval, dt, tc) {
  var factor = 2 * 3.1416 * 1/tc * dt;
  if (factor > 1)
    factor = 1;
  return (oldval * (1 - factor) + newval * factor);
}

var last_millis = Date.now()
var raw_smoothed = 0

function do_tare () {
  delta = raw_smoothed - cal1_raw
  cal1_raw = raw_smoothed
  cal2_raw += delta
}

function do_cal10 () {
  cal2_raw = raw_smoothed
}

var count = 0
function handle_weight(ev) {
  var buf = ev.target.value;
  var a = buf.getUint8(0);
  var b = buf.getUint8(1);
  var c = buf.getUint8(2);
  var d = buf.getUint8(3);
  var rawval = a | (b << 8) | (c << 16) | (d << 24)
  /* i think javascript promises to do bit operations 32 bit signed */
  $("#rawval").html(rawval);

  var millis = Date.now()
  var dt = (millis - last_millis) / 1000.0
  last_millis = millis

  tc = 10
  raw_smoothed = lpf_step (raw_smoothed, rawval, dt, tc)
  $("#rawval_smoothed").html(raw_smoothed.toFixed(0))

  var grams = raw_to_grams (rawval);
  $("#grams").html(grams.toFixed(1));
}

var device = null;
var server = null;
var services = null;

//;    return (server.getPrimaryServices());

async function do_connect () {
  try {
    var name_filter = {
      name: 'autotare'
    };

    var service_filter = {
      services: [autotare_service_uuid]
    };

    device = await navigator.bluetooth.requestDevice({
      filters: [name_filter],
      optionalServices: [autotare_service_uuid]
    });
    
    console.log(device);
    console.log("connected to", device.name);
    
    server = await device.gatt.connect();
    
    console.log ("xserver", server);
    
    services = await server.getPrimaryServices();
    console.log(services);

    console.log ("getting service");
    service = await server.getPrimaryService(autotare_service_uuid);
    console.log("service", service);
    
    characteristic = await service.getCharacteristic(autotare_weight_uuid);
    console.log("characteristic", characteristic);
    
    await characteristic.startNotifications();
    
    characteristic.addEventListener('characteristicvaluechanged',
				    handle_weight);
    console.log("notifications started")
  } catch (error) {
    console.log(error);
  }
}

async function do_disconnect () {
  try {
    if (device && device.gatt.connected) {
      device.gatt.disconnect()
    }
  } catch (error) {
    console.log(error);
  }
}

$(function () {
  $("#connect").click(do_connect)
  $("#disconnect").click(do_disconnect)
  $("#tare").click(do_tare)
  $("#cal10").click(do_cal10)
});
