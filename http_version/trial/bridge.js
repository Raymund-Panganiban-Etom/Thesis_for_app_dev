const WebSocket = require('ws');
const axios = require('axios');
const http = require('http');

const XIAOZHI_OTA_URL = 'https://api.tenclass.net/xiaozhi/ota/'; 
const DEVICE_ID = 'D4:06:06:B6:A9:FA'; 
const CLIENT_ID = '9863936e-7d01-44e6-bc80-a2d43ee6233e'; 
const BRIDGE_PORT = 8081;

const server = http.createServer();
const wss = new WebSocket.Server({ server });

wss.on('connection', (browserWs) => {
    console.log('✅ Browser connected. Initiating OTA handshake...');

    const payload = {
        "application": { "name": "xiaozhi", "version": "1.6.6" },
        "board": { "type": "esp32-s3", "mac": DEVICE_ID },
        "mac_address": DEVICE_ID 
    };

    axios.post(XIAOZHI_OTA_URL, payload, {
        headers: { 
            'Device-Id': DEVICE_ID, 
            'Client-Id': CLIENT_ID,
            'Content-Type': 'application/json', 
            'Activation-Version': '1',
            'User-Agent': 'esp-box-3/1.5.6'
        }
    }).then(response => {
        const data = response.data;
        
        if (data.activation && data.activation.code) {
            console.log(`\n⚠️ Not activated. Code: ${data.activation.code}\n`);
            return;
        }

        const wsUrl = data.websocket.url;
        const token = data.websocket.token;
        console.log(`🔗 Connecting to XiaoZhi AI Server at: ${wsUrl}`);

        const xiaozhiWs = new WebSocket(wsUrl, {
            headers: {
                'Authorization': `Bearer ${token}`,
                'Device-Id': DEVICE_ID,
                'Client-Id': CLIENT_ID,
                'User-Agent': 'esp-box-3/1.5.6',
                'Protocol-Version': '1' // CRITICAL HEADER
            }
        });

        xiaozhiWs.on('open', () => console.log('✅ Connected to XiaoZhi AI Server!'));
        
        xiaozhiWs.on('message', (data) => {
            // Log text messages to terminal for debugging
            if (typeof data === 'string' || (data instanceof Buffer && data.toString().trim().startsWith('{'))) {
                console.log('⬇️ Server:', data.toString());
            }
            if (browserWs.readyState === WebSocket.OPEN) browserWs.send(data);
        });
        
        browserWs.on('message', (data) => {
            console.log('⬆️ Browser:', data.toString());
            if (xiaozhiWs.readyState === WebSocket.OPEN) xiaozhiWs.send(data);
        });

        xiaozhiWs.on('close', (code, reason) => {
            console.log(`\n❌ XiaoZhi closed. Code: ${code}, Reason: ${reason.toString() || 'None'}`);
            browserWs.close();
        });
        
        xiaozhiWs.on('error', (err) => console.error('❌ XiaoZhi WS error:', err.message));

    }).catch(err => {
        console.error('❌ OTA handshake failed:', err.message);
        browserWs.close();
    });
});

server.listen(BRIDGE_PORT, () => {
    console.log(`🚀 Bridge server running on ws://localhost:${BRIDGE_PORT}`);
});