// Script per la gestione delle citazioni
document.addEventListener('DOMContentLoaded', function() {
  const quotesContent = document.getElementById('quotes-content');
  
  // Carica le categorie e le citazioni esistenti
  loadQuotes();
  
  // Prepara l'interfaccia per aggiungere/eliminare citazioni
  quotesContent.innerHTML = `
    <div class="quotes-manager">
      <h1>Gestione Citazioni</h1>
      
      <div class="add-quote-form">
        <h2>Aggiungi una nuova citazione</h2>
        <div class="form-group">
          <label for="quote-category">Categoria:</label>
          <select id="quote-category">
            <option value="pioggia">Pioggia</option>
            <option value="sereno">Sereno</option>
            <option value="nuvoloso">Nuvoloso</option>
            <option value="temporale">Temporale</option>
            <option value="neve">Neve</option>
            <option value="nebbia">Nebbia</option>
            <option value="any">Qualsiasi (generico)</option>
          </select>
        </div>
        <div class="form-group">
          <label for="quote-text">Citazione:</label>
          <textarea id="quote-text" rows="4" placeholder="Inserisci qui la tua citazione..."></textarea>
        </div>
        <button id="add-quote-btn" class="action-button">Aggiungi Citazione</button>
        <div id="add-message" class="message"></div>
      </div>
      
      <div class="quotes-list">
        <h2>Citazioni Salvate</h2>
        <div id="quotes-container"></div>
      </div>
      
      <div class="navigation">
        <a href="/" class="nav-button">Torna alla Pagina Principale</a>
      </div>
    </div>
  `;
  
  // Aggiungi event listener per il pulsante di aggiunta citazioni
  document.getElementById('add-quote-btn').addEventListener('click', addQuote);
});

// Carica le citazioni esistenti dal server
function loadQuotes() {
  fetch('/api/quotes/all')
    .then(response => response.json())
    .then(data => {
      displayQuotes(data);
    })
    .catch(error => {
      console.error('Errore nel caricamento delle citazioni:', error);
      document.getElementById('quotes-container').innerHTML = `
        <div class="error-message">
          <p>Impossibile caricare le citazioni. ${error.message}</p>
        </div>
      `;
    });
}

// Visualizza le citazioni organizzate per categoria
function displayQuotes(quotesData) {
  const container = document.getElementById('quotes-container');
  container.innerHTML = '';
  
  if (Object.keys(quotesData).length === 0) {
    container.innerHTML = '<p>Nessuna citazione trovata. Aggiungine una!</p>';
    return;
  }
  
  // Ordina le categorie alfabeticamente
  const categories = Object.keys(quotesData).sort();
  
  categories.forEach(category => {
    const quotes = quotesData[category];
    if (quotes && quotes.length > 0) {
      const categorySection = document.createElement('div');
      categorySection.className = 'quote-category';
      
      categorySection.innerHTML = `
        <h3>${formatCategoryName(category)} (${quotes.length})</h3>
        <div class="quotes-category-list" id="category-${category}"></div>
      `;
      
      container.appendChild(categorySection);
      
      const quotesList = document.getElementById(`category-${category}`);
      quotes.forEach((quote, index) => {
        const quoteElement = document.createElement('div');
        quoteElement.className = 'quote-item';
        quoteElement.innerHTML = `
          <p>${quote}</p>
          <button class="delete-quote-btn" data-category="${category}" data-index="${index}">
            Elimina
          </button>
        `;
        quotesList.appendChild(quoteElement);
      });
    }
  });
  
  // Aggiungi event listener per i pulsanti di eliminazione
  document.querySelectorAll('.delete-quote-btn').forEach(button => {
    button.addEventListener('click', function() {
      deleteQuote(this.dataset.category, this.dataset.index);
    });
  });
}

// Aggiunge una nuova citazione
function addQuote() {
  const category = document.getElementById('quote-category').value;
  const quoteText = document.getElementById('quote-text').value.trim();
  const messageElement = document.getElementById('add-message');
  
  if (!quoteText) {
    messageElement.textContent = 'Per favore inserisci una citazione.';
    messageElement.className = 'message error';
    return;
  }
  
  // Invia la citazione al server
  fetch('/api/quotes', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
    },
    body: JSON.stringify({
      category: category,
      quote: quoteText
    })
  })
  .then(response => {
    if (response.ok) {
      messageElement.textContent = 'Citazione aggiunta con successo!';
      messageElement.className = 'message success';
      document.getElementById('quote-text').value = '';
      
      // Ricarica le citazioni
      loadQuotes();
    } else {
      throw new Error('Errore nell\'aggiunta della citazione');
    }
  })
  .catch(error => {
    messageElement.textContent = `Errore: ${error.message}`;
    messageElement.className = 'message error';
  });
}

// Elimina una citazione
function deleteQuote(category, index) {
  if (confirm('Sei sicuro di voler eliminare questa citazione?')) {
    fetch('/api/quotes/delete', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({
        category: category,
        index: parseInt(index)
      })
    })
    .then(response => {
      if (response.ok) {
        // Ricarica le citazioni
        loadQuotes();
      } else {
        throw new Error('Errore nell\'eliminazione della citazione');
      }
    })
    .catch(error => {
      alert(`Errore: ${error.message}`);
    });
  }
}

// Formatta il nome della categoria per la visualizzazione
function formatCategoryName(category) {
  if (category === 'any') return 'Generiche';
  
  // Capitalizza la prima lettera
  return category.charAt(0).toUpperCase() + category.slice(1);
}
