// Funzione per salvare una citazione
function saveQuote() {
  const quote = document.getElementById('quote').value;
  const author = document.getElementById('author').value;
  const category = document.getElementById('category').value;
  const statusMsg = document.getElementById('status-message');
  
  if (!quote || !author) {
    statusMsg.innerHTML = '<div class="feedback error">Citazione e autore sono campi obbligatori</div>';
    return;
  }
  
  statusMsg.innerHTML = '<div class="feedback">Invio in corso...</div>';
  
  // Prepara i dati in formato JSON
  const combinedQuote = '"' + quote + '" - ' + author;
  const data = {
    cat: category,
    quote: combinedQuote
  };
  
  fetch('/api/quotes', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(data)
  })
  .then(response => response.json())
  .then(result => {
    if (result.success) {
      statusMsg.innerHTML = '<div class="feedback success">Citazione salvata con successo!</div>';
      document.getElementById('quote').value = '';
      document.getElementById('author').value = '';
      loadQuotes(); // Ricarica le citazioni dopo l'aggiunta
    } else {
      statusMsg.innerHTML = '<div class="feedback error">Errore: ' + (result.message || 'Impossibile salvare la citazione') + '</div>';
    }
  })
  .catch(error => {
    statusMsg.innerHTML = '<div class="feedback error">Errore di connessione</div>';
    console.error('Errore:', error);
  });
}

// Funzione per caricare le citazioni
function loadQuotes() {
  const quoteList = document.getElementById('quote-list');
  quoteList.innerHTML = '<div class="no-quotes">Caricamento...</div>';
  
  fetch('/api/quotes/list')
    .then(response => response.json())
    .then(data => {
      if (data.success && data.quotes) {
        let html = '';
        let isEmpty = true;
        
        // Cicla per ogni categoria
        for (const category in data.quotes) {
          const quotes = data.quotes[category];
          if (quotes && quotes.length > 0) {
            isEmpty = false;
            const displayName = getCategoryDisplayName(category);
            html += '<div class="category-header">' + displayName + '</div>';
            
            quotes.forEach((quote, index) => {
              html += '<div class="quote-item">';
              html += '<div class="quote-text">' + quote + '</div>';
              html += '<button class="delete-btn" onclick="deleteQuote(\'' + category + '\', ' + index + ')">&times; Elimina</button>';
              html += '</div>';
            });
          }
        }
        
        if (isEmpty) {
          quoteList.innerHTML = '<div class="no-quotes">Nessuna citazione salvata</div>';
        } else {
          quoteList.innerHTML = html;
        }
      } else {
        quoteList.innerHTML = '<div class="no-quotes">Nessuna citazione trovata</div>';
      }
    })
    .catch(error => {
      console.error('Errore nel caricamento delle citazioni:', error);
      quoteList.innerHTML = '<div class="no-quotes">Errore nel caricamento delle citazioni</div>';
    });
}

// Funzione per eliminare una citazione
function deleteQuote(category, index) {
  if (!confirm('Sei sicuro di voler eliminare questa citazione?')) return;
  
  const statusMsg = document.getElementById('status-message');
  statusMsg.innerHTML = '<div class="feedback">Eliminazione in corso...</div>';
  
  fetch('/api/quotes/delete', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ cat: category, index: index })
  })
  .then(response => response.json())
  .then(result => {
    if (result.success) {
      statusMsg.innerHTML = '<div class="feedback success">Citazione eliminata con successo!</div>';
      loadQuotes(); // Ricarica l'elenco
    } else {
      statusMsg.innerHTML = '<div class="feedback error">Errore: ' + (result.message || 'Impossibile eliminare la citazione') + '</div>';
    }
  })
  .catch(error => {
    statusMsg.innerHTML = '<div class="feedback error">Errore di connessione</div>';
    console.error('Errore:', error);
  });
}

// Funzione per ottenere il nome visualizzato delle categorie
function getCategoryDisplayName(category) {
  const categoryMap = {
    sunny: 'Soleggiato',
    cloudy: 'Nuvoloso',
    rainy: 'Piovoso',
    snowy: 'Nevoso',
    any: 'Qualsiasi'
  };
  return categoryMap[category] || category;
}

// Carica le citazioni all'avvio della pagina
document.addEventListener('DOMContentLoaded', loadQuotes);
