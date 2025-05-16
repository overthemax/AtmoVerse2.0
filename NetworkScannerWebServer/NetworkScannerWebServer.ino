#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClient.h>

// WiFi credentials
const char* ssid = "WIFI Casa";     // Replace with your WiFi name
const char* password = "Dragonero1";  // Replace with your WiFi password

// Fixed IP configuration
IPAddress staticIP(192, 168, 0, 233);    // Your desired fixed IP address
IPAddress gateway(192, 168, 0, 1);       // Your router's IP address
IPAddress subnet(255, 255, 255, 0);      // Subnet mask
IPAddress dns(8, 8, 8, 8);               // DNS (Google's DNS server)

WebServer server(80);  // Web server on port 80

void setup() {
  Serial.begin(115200);
  Serial.println("\nStarting...");
  
  // Configure static IP
  if (!WiFi.config(staticIP, gateway, subnet, dns)) {
    Serial.println("STA Failed to configure");
  }
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println();
  Serial.print("Connected to WiFi: ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  // Set up web server routes
  server.on("/", handleRoot);
  server.on("/scan", handleScan);
  server.on("/scanwifi", HTTP_GET, performWiFiScan);
  server.onNotFound(handleNotFound);
  
  // Start the server
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
}

// Handle root URL
void handleRoot() {
  String html = getHTMLPage();
  server.send(200, "text/html", html);
}

// Handle WiFi scan request
void performWiFiScan() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  
  Serial.println("WiFi scan requested from web interface");
  String scanResult = scanWiFiNetworks();
  server.send(200, "application/json", scanResult);
  Serial.println("WiFi scan results sent to client");
}

// Handle /scan URL (redirect to root)
void handleScan() {
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

// Handle 404 not found
void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  
  server.send(404, "text/plain", message);
}

// Get the HTML page
String getHTMLPage() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>WiFi Network Scanner</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body {
      font-family: Arial, sans-serif;
      margin: 20px;
      background-color: #f5f5f5;
    }
    .container {
      max-width: 800px;
      margin: 0 auto;
      background: white;
      padding: 20px;
      border-radius: 8px;
      box-shadow: 0 2px 4px rgba(0,0,0,0.1);
    }
    h1 {
      color: #333;
      text-align: center;
    }
    button {
      background-color: #4CAF50;
      color: white;
      padding: 10px 15px;
      border: none;
      border-radius: 4px;
      cursor: pointer;
      font-size: 16px;
      display: block;
      margin: 20px auto;
    }
    button:hover {
      background-color: #45a049;
    }
    button:disabled {
      background-color: #cccccc;
      cursor: not-allowed;
    }
    #results {
      margin-top: 20px;
      border-top: 1px solid #ddd;
      padding-top: 10px;
    }
    table {
      width: 100%;
      border-collapse: collapse;
    }
    th, td {
      border: 1px solid #ddd;
      padding: 8px;
      text-align: left;
    }
    th {
      background-color: #f2f2f2;
    }
    tr:nth-child(even) {
      background-color: #f9f9f9;
    }
    .status {
      text-align: center;
      color: #666;
      font-style: italic;
    }
    .signal-strength {
      display: inline-block;
      width: 50px;
      height: 10px;
      background-color: #ddd;
      margin-right: 5px;
      vertical-align: middle;
    }
    .signal-strength-fill {
      height: 100%;
      background-color: #4CAF50;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>WiFi Network Scanner</h1>
    <p class="status">IP Address: <span id="ipAddress"></span></p>
    <button id="scanButton" onclick="scanWiFi()">Scan WiFi Networks</button>
    <div id="results">
      <p class="status" id="scanStatus">Click the button to scan for WiFi networks</p>
      <table id="networkTable" style="display:none">
        <thead>
          <tr>
            <th>SSID</th>
            <th>Signal Strength</th>
            <th>Channel</th>
            <th>Encryption</th>
          </tr>
        </thead>
        <tbody id="networkTableBody">
        </tbody>
      </table>
    </div>
  </div>

  <script>
    // Set the current IP address
    document.getElementById('ipAddress').textContent = window.location.hostname;
    
    // Function to scan WiFi networks
    function scanWiFi() {
      const scanButton = document.getElementById('scanButton');
      const scanStatus = document.getElementById('scanStatus');
      const networkTable = document.getElementById('networkTable');
      const networkTableBody = document.getElementById('networkTableBody');
      
      // Disable button during scan
      scanButton.disabled = true;
      scanStatus.textContent = "Scanning WiFi networks, please wait...";
      networkTable.style.display = "none";
      
      fetch('/scanwifi')
        .then(response => {
          if (!response.ok) {
            throw new Error(`HTTP error! status: ${response.status}`);
          }
          return response.json();
        })
        .then(data => {
          networkTableBody.innerHTML = '';
          
          console.log("Scan results:", data);
          
          if (data.networks && data.networks.length > 0) {
            data.networks.forEach(network => {
              const row = document.createElement('tr');
              
              // Create signal strength indicator
              const signalPercent = Math.min(100, Math.max(0, 2 * (network.rssi + 100)));
              const signalBar = `
                <div class="signal-strength">
                  <div class="signal-strength-fill" style="width: ${signalPercent}%;"></div>
                </div>
                ${network.rssi} dBm (${signalPercent.toFixed(0)}%)
              `;
              
              row.innerHTML = `
                <td>${network.ssid || '<Hidden Network>'}</td>
                <td>${signalBar}</td>
                <td>${network.channel}</td>
                <td>${network.encryption}</td>
              `;
              networkTableBody.appendChild(row);
            });
            
            scanStatus.textContent = `Scan completed. Found ${data.networks.length} WiFi networks.`;
            networkTable.style.display = "table";
          } else {
            scanStatus.textContent = "Scan completed. No WiFi networks found.";
          }
        })
        .catch(error => {
          console.error("Error during scan:", error);
          scanStatus.textContent = "Error scanning WiFi networks: " + error.message;
        })
        .finally(() => {
          // Re-enable button after scan
          scanButton.disabled = false;
        });
    }
  </script>
</body>
</html>
)rawliteral";
  return html;
}

// Scan WiFi Networks
String scanWiFiNetworks() {
  Serial.println("Starting WiFi scan...");
  
  // Delete old scan results
  WiFi.scanDelete();
  
  // Start scan
  int networksFound = WiFi.scanNetworks();
  Serial.println("Scan done");
  
  // Prepare JSON response
  String result = "{\"networks\":[";
  
  if (networksFound == 0) {
    Serial.println("No networks found");
  } else {
    Serial.print(networksFound);
    Serial.println(" networks found");
    
    for (int i = 0; i < networksFound; ++i) {
      // Add comma if not first network
      if (i > 0) {
        result += ",";
      }
      
      // Get encryption type
      String encType = "";
      switch (WiFi.encryptionType(i)) {
        case WIFI_AUTH_OPEN:
          encType = "Open";
          break;
        case WIFI_AUTH_WEP:
          encType = "WEP";
          break;
        case WIFI_AUTH_WPA_PSK:
          encType = "WPA-PSK";
          break;
        case WIFI_AUTH_WPA2_PSK:
          encType = "WPA2-PSK";
          break;
        case WIFI_AUTH_WPA_WPA2_PSK:
          encType = "WPA/WPA2-PSK";
          break;
        case WIFI_AUTH_WPA2_ENTERPRISE:
          encType = "WPA2-Enterprise";
          break;
        default:
          encType = "Unknown";
      }
      
      // Add network to JSON
      result += "{";
      result += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
      result += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
      result += "\"channel\":" + String(WiFi.channel(i)) + ",";
      result += "\"encryption\":\"" + encType + "\"";
      result += "}";
      
      // Print to serial
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(" dBm) Ch:");
      Serial.print(WiFi.channel(i));
      Serial.print(" Enc:");
      Serial.println(encType);
    }
  }
  
  result += "]}";
  return result;
}
