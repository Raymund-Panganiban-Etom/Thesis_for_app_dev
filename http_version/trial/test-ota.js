const axios = require('axios');

const XIAOZHI_OTA_URL = 'https://api.tenclass.net/xiaozhi/ota/';

// Use a valid, colon-separated MAC address (Espressif OUI)
const DEVICE_ID = 'D4:06:06:B6:A9:FA'; 
const CLIENT_ID = '9863936e-7d01-44e6-bc80-a2d43ee6233e';

const payload = {
    "application": { 
        "name": "xiaozhi",
        "version": "1.6.6" 
    },
    "board": { 
        "type": "esp32-s3",
        "mac": DEVICE_ID 
    },
    // This field is required and must match the Device-Id header
    "mac_address": DEVICE_ID 
};

axios.post(XIAOZHI_OTA_URL, payload, {
    headers: {
        'Device-Id': DEVICE_ID,
        'Client-Id': CLIENT_ID,
        'Content-Type': 'application/json',
        'Activation-Version': '1',
        'User-Agent': 'esp-box-3/1.5.6' // Required by the server
    }
}).then(res => {
    console.log('✅ Server Response:');
    console.log(JSON.stringify(res.data, null, 2));

    if (res.data.activation && res.data.activation.code) {
        console.log(`\n🎯 YOUR ACTIVATION CODE: ${res.data.activation.code}`);
    } else {
        console.log('\n⚠️ No activation field. Device may already be bound.');
    }
}).catch(err => {
    console.error('❌ Error:', err.message);
    if (err.response) console.error('Server Data:', err.response.data);
});