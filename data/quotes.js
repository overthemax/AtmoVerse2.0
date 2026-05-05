// JavaScript per gestire l'invio delle citazioni
function saveQuote() {
  const quote = document.getElementById('quote').value;
  const author = document.getElementById('author').value;
  const category = document.getElementById('category').value;
  const statusMsg = document.getElementById('status-message');
  
  // Validazione
  if (!quote || !author) {
    statusMsg.innerHTML = '<div style="color:red">Citazione e autore sono campi obbligatori</div>';
    return;
  }
  
  statusMsg.innerHTML = '<div style="color:#8d6e46">Invio in corso...</div>';
  
  // Prepara i dati in formato JSON
  const combinedQuote = '"' + quote + '" - ' + author;
  const data = {
    cat: category,
    quote: combinedQuote
  };
  
  // Invia i dati all'endpoint API corretto
  fetch('/api/quotes', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json'
    },
    body: JSON.stringify(data)
  })
  .then(response => response.json())
  .then(result => {
    if (result.success) {
      statusMsg.innerHTML = '<div style="color:green">Citazione salvata con successo!</div>';
      document.getElementById('quote').value = '';
      document.getElementById('author').value = '';
    } else {
      statusMsg.innerHTML = '<div style="color:red">Errore: ' + (result.message || 'Impossibile salvare la citazione') + '</div>';
    }
  })
  .catch(error => {
    statusMsg.innerHTML = '<div style="color:red">Errore di connessione</div>';
    console.error('Errore:', error);
  });
}
