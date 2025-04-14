// server.js
const express = require('express');
const bodyParser = require('body-parser');
const morgan = require('morgan');
const fs = require('fs');
const path = require('path');
const app = express();
const port = process.env.PORT || 3000;

// Create data directory if it doesn't exist
const dataDir = path.join(__dirname, 'data');
if (!fs.existsSync(dataDir)) {
  fs.mkdirSync(dataDir);
}

// Configure middleware
app.use(bodyParser.json());
app.use(bodyParser.urlencoded({ extended: true }));
app.use(morgan('combined')); // Log all requests

// Store registered readers and their data
const readers = {};

// Track last API call
let lastApiCall = {
  endpoint: null,
  method: null,
  params: null,
  timestamp: null
};

// Helper function to record API calls
function recordApiCall(endpoint, method, params) {
  lastApiCall = {
    endpoint,
    method,
    params,
    timestamp: new Date().toISOString()
  };
}

// QR Reader Registration/Heartbeat endpoint
app.put('/api/node/qrCodeReaderHeartbeat', (req, res) => {
  const nodeUUID = req.query.nodeUUID || 'unknown';
  const qrReaderUUID = req.query.qrReaderUUID || 'unknown';
  const errorMessages = req.query.errorMessages;
  
  const timestamp = new Date().toISOString();
  
  // Record this API call
  recordApiCall('/api/node/qrCodeReaderHeartbeat', 'PUT', {
    nodeUUID,
    qrReaderUUID,
    errorMessages
  });
  
  // Store/update reader information
  const readerKey = `${nodeUUID}-${qrReaderUUID}`;
  console.log(`Storing reader with key: ${readerKey}`);
  
  readers[readerKey] = {
    lastHeartbeat: timestamp,
    errorMessages: errorMessages || null
  };
  
  console.log(`Heartbeat received from node ${nodeUUID}, reader ${qrReaderUUID}`);
  
  // Log to file
  const logEntry = {
    type: 'heartbeat',
    timestamp,
    nodeUUID,
    qrReaderUUID,
    errorMessages
  };
  
  fs.appendFileSync(
    path.join(dataDir, 'heartbeats.log'),
    JSON.stringify(logEntry) + '\n'
  );
  
  res.status(200).json({ status: 'ok', timestamp });
});

// QR Code Data Submission endpoint
app.put('/api/node/recordQRCode', (req, res) => {
  const nodeUUID = req.query.nodeUUID || 'unknown';
  const qrReaderUUID = req.query.qrReaderUUID || 'unknown';
  const data = req.query.data;
  
  const timestamp = new Date().toISOString();
  
  if (!data) {
    return res.status(400).json({ status: 'error', message: 'No QR code data provided' });
  }
  
  // Record this API call
  recordApiCall('/api/node/recordQRCode', 'PUT', {
    nodeUUID,
    qrReaderUUID,
    data
  });
  
  console.log(`QR Code received from node ${nodeUUID}, reader ${qrReaderUUID}: ${data}`);
  
  // Log to file
  const logEntry = {
    type: 'qrcode',
    timestamp,
    nodeUUID,
    qrReaderUUID,
    data
  };
  
  fs.appendFileSync(
    path.join(dataDir, 'qrcodes.log'),
    JSON.stringify(logEntry) + '\n'
  );
  
  res.status(200).json({ status: 'ok', timestamp });
});

// Simple status page
app.get('/', (req, res) => {
  const readerInfo = Object.entries(readers).map(([id, info]) => {
    const lastHyphenIndex = id.lastIndexOf('-');
    const nodeUUID = id.substring(0, lastHyphenIndex);
    const qrReaderUUID = id.substring(lastHyphenIndex + 1);
    
    return {
      nodeUUID,
      qrReaderUUID,
      lastHeartbeat: info.lastHeartbeat,
      errorMessages: info.errorMessages
    };
  });
  
  res.send(`
    <html>
    <head>
      <title>Virtual Node Service</title>
      <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        pre { background: #f5f5f5; padding: 10px; border-radius: 5px; }
        .api-call { margin-bottom: 20px; }
        .no-call { color: #666; font-style: italic; }
        h2 { margin-top: 30px; border-bottom: 1px solid #ddd; padding-bottom: 10px; }
        table { border-collapse: collapse; width: 100%; margin-bottom: 20px; }
        th { background-color: #f2f2f2; text-align: left; }
        td, th { padding: 8px; border: 1px solid #ddd; }
        tr:nth-child(even) { background-color: #f9f9f9; }
        tr:hover { background-color: #f2f2f2; }
        small { color: #666; }
      </style>
      <meta http-equiv="refresh" content="10">
    </head>
    <body>
      <h1>Virtual Node Service</h1>
      
      <h2>Last API Call</h2>
      <div class="api-call">
        ${lastApiCall.endpoint 
          ? `<p><strong>Endpoint:</strong> ${lastApiCall.endpoint} (${lastApiCall.method})</p>
             <p><strong>Time:</strong> ${lastApiCall.timestamp}</p>
             <p><strong>Parameters:</strong></p>
             <pre>${JSON.stringify(lastApiCall.params, null, 2)}</pre>`
          : '<p class="no-call">No API calls received yet</p>'
        }
      </div>
      
      <h2>Registered QR Code Readers</h2>
      <div class="readers">
        ${readerInfo.length > 0 
          ? `<table border="1" cellpadding="5" cellspacing="0">
              <tr>
                <th>Node UUID</th>
                <th>QR Reader UUID</th>
                <th>Last Heartbeat</th>
                <th>Error Messages</th>
              </tr>
              ${readerInfo.map(reader => `
                <tr>
                  <td>${reader.nodeUUID}</td>
                  <td>${reader.qrReaderUUID}</td>
                  <td>${new Date(reader.lastHeartbeat).toLocaleString()}</td>
                  <td>${reader.errorMessages || 'None'}</td>
                </tr>
              `).join('')}
            </table>
            <p><small>Debug info: Raw stored keys: ${Object.keys(readers).join(', ')}</small></p>`
          : '<p>No readers registered yet</p>'
        }
      </div>
      
      <h2>API Endpoints</h2>
      <ul>
        <li>PUT /api/node/qrCodeReaderHeartbeat?nodeUUID={nodeId}&qrReaderUUID={scannerId}</li>
        <li>PUT /api/node/recordQRCode?nodeUUID={nodeId}&qrReaderUUID={scannerId}&data={qrData}</li>
      </ul>
    </body>
    </html>
  `);
});

app.listen(port, '0.0.0.0', () => {
  console.log(`Virtual Node service listening on port ${port}`);
}); 