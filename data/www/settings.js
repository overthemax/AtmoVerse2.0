/**
 * AtmoVerse 2.0 - Script impostazioni
 * Gestisce l'interfaccia web delle impostazioni
 */

// Elementi DOM
const settingsForm = document.getElementById('settingsForm');
const ssidSelect = document.getElementById('ssid');
const passwordInput = document.getElementById('password');
const cityInput = document.getElementById('city');
const timezoneSelect = document.getElementById('timezone');
const daylightSavingCheckbox = document.getElementById('daylightSaving');
const ntpServerSelect = document.getElementById('ntpServer');
const statusMessage = document.getElementById('status-message');
const deviceModeElement = document.getElementById('deviceMode');
const ipAddressElement = document.getElementById('ipAddress');

// Gestione delle impostazioni AtmoVerse
let currentConfig = {};

// Carica le impostazioni all'avvio
document.addEventListener('DOMContentLoaded', loadSettings);

// Funzione per caricare le impostazioni attuali
async function loadSettings() {
    try {
        const response = await fetch('/api/config');
        if (!response.ok) throw new Error('Errore nel caricamento delle impostazioni');
        
        currentConfig = await response.json();
        console.log('Config caricata:', currentConfig);
        
        // Popola i campi del form
        if (currentConfig.ssid) {
            // Crea un'opzione per l'SSID corrente
            const currentOption = document.createElement('option');
            currentOption.value = currentConfig.ssid;
            currentOption.textContent = currentConfig.ssid + ' (Attuale)';
            currentOption.selected = true;
            ssidSelect.appendChild(currentOption);
        }
        
        cityInput.value = currentConfig.city || '';
        timezoneSelect.value = currentConfig.timezone || '1';
        daylightSavingCheckbox.checked = currentConfig.daylightSaving || false;
        ntpServerSelect.value = currentConfig.ntpServer || 'pool.ntp.org';
        
        // Aggiorna le informazioni del dispositivo
        updateDeviceInfo();
        
        // Esegui una scansione iniziale delle reti WiFi
        scanWiFi();
        
    } catch (error) {
        console.error('Errore:', error);
        showMessage('Errore nel caricamento delle impostazioni', 'error');
    }
}

// Funzione per salvare le impostazioni
async function saveSettings(event) {
    event.preventDefault();
    
    const formData = {
        ssid: ssidSelect.value,
        password: passwordInput.value,
        city: cityInput.value,
        timezone: parseInt(timezoneSelect.value),
        daylightSaving: daylightSavingCheckbox.checked,
        ntpServer: ntpServerSelect.value
    };
    
    console.log('Invio configurazione:', formData);
    
    try {
        const response = await fetch('/api/config', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(formData)
        });
        
        const result = await response.json();
        
        if (result.success) {
            showMessage('Impostazioni salvate con successo', 'success');
            passwordInput.value = '';
            loadSettings();
        } else {
            throw new Error(result.message || 'Errore nel salvataggio');
        }
    } catch (error) {
        console.error('Errore:', error);
        showMessage(error.message, 'error');
    }
}

// Funzione per scansionare le reti WiFi
async function scanWiFi() {
    const scanButton = document.querySelector('.wifi-scan-button');
    console.log('Avvio scansione WiFi');
    
    try {
        // Cambia stato pulsante
        scanButton.disabled = true;
        scanButton.innerHTML = '<i class="fas fa-spinner fa-spin"></i> Scansione...';
        
        // Salva SSID corrente
        const currentSsid = ssidSelect.value;
        
        // Effettua la richiesta di scansione
        console.log('Richiesta scansione a /api/wifi/scan');
        const response = await fetch('/api/wifi/scan');
        
        if (!response.ok) {
            console.error('Risposta non valida:', response);
            throw new Error('Errore durante la scansione');
        }
        
        // Converti risposta in JSON
        const networkData = await response.text();
        console.log('Dati ricevuti:', networkData);
        
        // Parsa il JSON
        const networks = JSON.parse(networkData);
        console.log('Reti trovate:', networks);
        
        // Pulisci e ripopola il select
        ssidSelect.innerHTML = '<option value="">Seleziona una rete...</option>';
        
        if (networks && networks.length > 0) {
            networks
                .sort((a, b) => b.rssi - a.rssi)
                .forEach(network => {
                    const option = document.createElement('option');
                    option.value = network.ssid;
                    const signal = network.rssi >= -50 ? 'Ottimo' : 
                                network.rssi >= -60 ? 'Buono' : 
                                network.rssi >= -70 ? 'Discreto' : 'Debole';
                    option.textContent = `${network.ssid} (${signal}) ${network.secure ? '🔒' : ''}`;
                    ssidSelect.appendChild(option);
                });
            
            // Ripristina la selezione precedente se presente
            if (currentSsid) {
                const option = Array.from(ssidSelect.options).find(opt => opt.value === currentSsid);
                if (option) option.selected = true;
            }
            
            showMessage(`Trovate ${networks.length} reti WiFi`, 'success');
        } else {
            showMessage('Nessuna rete WiFi trovata', 'warning');
        }
    } catch (error) {
        console.error('Errore scansione:', error);
        showMessage('Errore durante la scansione WiFi', 'error');
    } finally {
        scanButton.disabled = false;
        scanButton.innerHTML = '<i class="fas fa-sync-alt"></i> Cerca reti';
    }
}

// Funzione per mostrare/nascondere la password
function togglePassword() {
    const input = document.getElementById('password');
    
    if (input.type === 'password') {
        input.type = 'text';
    } else {
        input.type = 'password';
    }
}

// Funzione per aggiornare le informazioni del dispositivo
function updateDeviceInfo() {
    deviceModeElement.textContent = 
        currentConfig.apMode ? 'Configurazione (AP)' : 'Normale (Client)';
    ipAddressElement.textContent = 
        currentConfig.ipAddress || '192.168.4.1';
}

// Funzione per mostrare messaggi
function showMessage(message, type) {
    const statusMsg = document.getElementById('status-message');
    if (!statusMsg) return;
    
    statusMsg.textContent = message;
    statusMsg.className = `status-message ${type}`;
    statusMsg.style.display = 'block';
    
    // Nascondi il messaggio dopo 5 secondi
    setTimeout(() => {
        statusMsg.style.display = 'none';
    }, 5000);
}

// Imposta i gestori eventi
settingsForm.addEventListener('submit', saveSettings);
