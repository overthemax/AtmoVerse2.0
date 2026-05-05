// Funzione per aggiornare i dati meteo
async function refreshWeather() {
    try {
        const response = await fetch('/api/weather');
        if (!response.ok) throw new Error('HTTP ' + response.status);
        const data = await response.json();
        updateWeatherUI(data);
        showMessage('Dati aggiornati', 'success');
    } catch (error) {
        console.error('Errore:', error);
        showMessage('Errore durante l\'aggiornamento', 'error');
    }
}

// Funzione per aggiornare l'interfaccia utente
function updateWeatherUI(data) {
    const w = data.weather || {};
    const loc = data.location || {};
    const t = data.time || {};

    // Aggiorna icona meteo
    const weatherIcon = document.querySelector('.weather-icon i');
    updateWeatherIcon(weatherIcon, w.icon);

    // Aggiorna descrizione/condizione
    document.getElementById('weatherDescription').textContent = w.condition || '—';

    // Aggiorna temperatura
    if (typeof w.temperature === 'number') {
        document.getElementById('temperature').textContent = `${w.temperature.toFixed(1)} °C`;
    } else {
        document.getElementById('temperature').textContent = '--';
    }

    // Aggiorna umidità
    if (typeof w.humidity === 'number') {
        document.getElementById('humidity').textContent = `${w.humidity} %`;
    } else {
        document.getElementById('humidity').textContent = '--';
    }

    // Aggiorna pressione
    if (typeof w.pressure === 'number') {
        document.getElementById('pressure').textContent = `${w.pressure} hPa`;
    } else {
        document.getElementById('pressure').textContent = '--';
    }

    // Aggiorna località
    document.getElementById('location').textContent = loc.city || '—';

    // Aggiorna timestamp ultimo aggiornamento (stringa fornita dal backend)
    document.getElementById('lastUpdate').textContent = w.last_update || (t.local || '--');
}

// Funzione per aggiornare l'icona meteo
function updateWeatherIcon(iconElement, weatherCode) {
    const iconMap = {
        '01d': 'sun',
        '01n': 'moon',
        '02d': 'cloud-sun',
        '02n': 'cloud-moon',
        '03d': 'cloud',
        '03n': 'cloud',
        '04d': 'clouds',
        '04n': 'clouds',
        '09d': 'cloud-showers-heavy',
        '09n': 'cloud-showers-heavy',
        '10d': 'cloud-sun-rain',
        '10n': 'cloud-moon-rain',
        '11d': 'bolt',
        '11n': 'bolt',
        '13d': 'snowflake',
        '13n': 'snowflake',
        '50d': 'smog',
        '50n': 'smog'
    };
    
    const iconClass = iconMap[weatherCode] || 'cloud';
    iconElement.className = `fas fa-${iconClass}`;
}

// Funzione per mostrare messaggi
function showMessage(message, type) {
    const messageElement = document.createElement('div');
    messageElement.className = `message ${type}`;
    messageElement.textContent = message;
    
    document.body.appendChild(messageElement);
    
    setTimeout(() => {
        messageElement.remove();
    }, 3000);
}

// Aggiorna i dati ogni 5 minuti
setInterval(refreshWeather, 5 * 60 * 1000);

// Carica i dati all'avvio
document.addEventListener('DOMContentLoaded', refreshWeather);
