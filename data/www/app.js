// Funzione per aggiornare i dati meteo
async function refreshWeather() {
    try {
        const response = await fetch('/api/weather');
        const data = await response.json();
        
        if (data.success) {
            updateWeatherUI(data);
            showMessage('Dati aggiornati con successo', 'success');
        } else {
            showMessage('Errore durante l\'aggiornamento dei dati', 'error');
        }
    } catch (error) {
        console.error('Errore:', error);
        showMessage('Errore di connessione', 'error');
    }
}

// Funzione per aggiornare l'interfaccia utente
function updateWeatherUI(data) {
    // Aggiorna icona meteo
    const weatherIcon = document.querySelector('.weather-icon i');
    updateWeatherIcon(weatherIcon, data.weather.icon);
    
    // Aggiorna descrizione
    document.getElementById('weatherDescription').textContent = data.weather.description;
    
    // Aggiorna temperatura
    document.getElementById('temperature').textContent = `${data.temperature.toFixed(1)} °C`;
    
    // Aggiorna umidità
    document.getElementById('humidity').textContent = `${data.humidity.toFixed(1)} %`;
    
    // Aggiorna pressione
    document.getElementById('pressure').textContent = `${data.pressure} hPa`;
    
    // Aggiorna località
    document.getElementById('location').textContent = data.city;
    
    // Aggiorna fuso orario
    document.getElementById('timezone').textContent = `UTC${data.timezone >= 0 ? '+' : ''}${data.timezone}`;
    
    // Aggiorna timestamp ultimo aggiornamento
    const lastUpdate = new Date(data.lastUpdate * 1000);
    document.getElementById('lastUpdate').textContent = lastUpdate.toLocaleTimeString();
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
