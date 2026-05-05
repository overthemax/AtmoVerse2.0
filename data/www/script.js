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
const apiKeyInput = document.getElementById('apiKey');
const hourFormatSelect = document.getElementById('hourFormat');
const unitsSelect = document.getElementById('units');
const languageSelect = document.getElementById('language');
const powerSavingEnabledCheckbox = document.getElementById('powerSavingEnabled');
const powerSavingStartHourInput = document.getElementById('powerSavingStartHour');
const powerSavingEndHourInput = document.getElementById('powerSavingEndHour');
const normalUpdateIntervalInput = document.getElementById('normalUpdateInterval');
const powerSavingUpdateIntervalInput = document.getElementById('powerSavingUpdateInterval');
const weatherUpdateIntervalInput = document.getElementById('weatherUpdateInterval');
const weatherPowerSavingUpdateIntervalInput = document.getElementById('weatherPowerSavingUpdateInterval');
const maxNetworkRetriesInput = document.getElementById('maxNetworkRetries');
const deviceModeElement = document.getElementById('deviceMode');
const ipAddressElement = document.getElementById('ipAddress');
const themeSelect = document.getElementById('theme');

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
        timezoneSelect.value = (currentConfig.timezone ?? 1).toString();
        daylightSavingCheckbox.checked = !!currentConfig.daylightSaving;
        ntpServerSelect.value = currentConfig.ntpServer || 'pool.ntp.org';
        apiKeyInput.value = currentConfig.api_key || '';
        hourFormatSelect.value = currentConfig.use24hFormat ? '24' : '12';
        unitsSelect.value = currentConfig.units || 'metric';
        languageSelect.value = currentConfig.language || 'it';
        if (themeSelect) themeSelect.value = currentConfig.theme || 'classic';
        powerSavingEnabledCheckbox.checked = !!currentConfig.powerSavingEnabled;
        powerSavingStartHourInput.value = currentConfig.powerSavingStartHour ?? 22;
        powerSavingEndHourInput.value = currentConfig.powerSavingEndHour ?? 7;
        normalUpdateIntervalInput.value = currentConfig.normalUpdateInterval ?? 30;
        powerSavingUpdateIntervalInput.value = currentConfig.powerSavingUpdateInterval ?? 120;
        weatherUpdateIntervalInput.value = currentConfig.weatherUpdateInterval ?? 30;
        weatherPowerSavingUpdateIntervalInput.value = currentConfig.weatherPowerSavingUpdateInterval ?? 120;
        maxNetworkRetriesInput.value = currentConfig.maxNetworkRetries ?? 3;
        
        // Aggiorna le informazioni del dispositivo
        updateDeviceInfo();
        
        // Evita la scansione automatica per non rallentare il caricamento
        
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
        ntpServer: ntpServerSelect.value,
        api_key: apiKeyInput.value,
        use24hFormat: hourFormatSelect.value === '24',
        units: unitsSelect.value,
        language: languageSelect.value,
        theme: themeSelect ? themeSelect.value : 'classic',
        powerSavingEnabled: powerSavingEnabledCheckbox.checked,
        powerSavingStartHour: parseInt(powerSavingStartHourInput.value || '22'),
        powerSavingEndHour: parseInt(powerSavingEndHourInput.value || '7'),
        normalUpdateInterval: parseInt(normalUpdateIntervalInput.value || '30'),
        powerSavingUpdateInterval: parseInt(powerSavingUpdateIntervalInput.value || '120'),
        weatherUpdateInterval: parseInt(weatherUpdateIntervalInput.value || '30'),
        weatherPowerSavingUpdateInterval: parseInt(weatherPowerSavingUpdateIntervalInput.value || '120'),
        maxNetworkRetries: parseInt(maxNetworkRetriesInput.value || '3')
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
        scanButton.textContent = '🔎 Scansione...';
        
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
        scanButton.textContent = '🔄 Cerca reti';
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
