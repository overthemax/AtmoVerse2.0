// Script per la gestione delle citazioni con editor formattazione e lista editabile
document.addEventListener('DOMContentLoaded', function() {
  const quotesContent = document.getElementById('quotes-content');

  // UI principale
  quotesContent.innerHTML = `
    <div class="quotes-manager">
      <h1>Gestione Citazioni</h1>

      <div class="quote-position card">
        <h2>Posizione Citazione sul Display</h2>
        <div class="form-group-inline">
          <label for="quote-pos-x">X (px):</label>
          <input id="quote-pos-x" type="number" min="0" max="600" step="1" placeholder="35" style="width:100px" />
          <label for="quote-pos-y" style="margin-left:12px">Y (px):</label>
          <input id="quote-pos-y" type="number" min="0" max="400" step="1" placeholder="190" style="width:100px" />
          <button id="save-quote-pos-btn" class="action-button" style="margin-left:12px">Salva Posizione</button>
        </div>
        <small style="color:#8a7a5e">Imposta le coordinate in pixel rispetto all'angolo in alto a sinistra dello schermo. Valori non validi verranno adattati automaticamente.</small>
        <div id="quote-pos-message" class="message" style="margin-top:8px"></div>
      </div>

      <div class="add-quote-form">
        <h2 id="form-title">Aggiungi una nuova citazione</h2>
        <div class="form-group">
          <label for="quote-category">Categoria:</label>
          <select id="quote-category">
            <option value="temporale">Temporale (OpenWeather: Thunderstorm)</option>
            <option value="pioggia_leggera">Pioggia leggera (OpenWeather: Drizzle, Light Rain)</option>
            <option value="pioggia">Pioggia (OpenWeather: Rain)</option>
            <option value="neve">Neve (OpenWeather: Snow)</option>
            <option value="cielo_sereno">Cielo sereno (OpenWeather: Clear)</option>
            <option value="poche_nuvole">Poche nuvole (OpenWeather: Few clouds)</option>
            <option value="nuvole_sparse">Nuvole sparse (OpenWeather: Scattered clouds)</option>
            <option value="nuvole_abbondanti">Nuvole abbondanti (OpenWeather: Broken clouds, Overcast)</option>
            <option value="nebbia">Nebbia (OpenWeather: Mist, Fog, Haze)</option>
            <option value="tempesta">Tempesta (OpenWeather: Extreme weather, Squall, Tornado)</option>
            <option value="any">Qualsiasi (generico)</option>
          </select>
          <small style="color:#8a7a5e">Le categorie tra parentesi sono i tipi di meteo di OpenWeather associati a ciascuna categoria UI.</small>
        </div>

        <div class="form-group">
          <label>Formato:</label>
          <div class="toolbar" id="quote-toolbar">
            <button type="button" data-cmd="bold"><b>B</b></button>
            <button type="button" data-cmd="italic"><i>I</i></button>
            <select id="font-size-select" title="Dimensione testo">
              <option value="16">Normale</option>
              <option value="20">Grande</option>
              <option value="24">Molto grande</option>
            </select>
          </div>
        </div>

        <div class="form-group">
          <label for="quote-editor">Citazione:</label>
          <div id="quote-editor" class="editor" contenteditable="true" placeholder="Scrivi qui la citazione..."></div>
        </div>

        <div class="form-group">
          <label for="quote-author">Autore (opzionale):</label>
          <input id="quote-author" type="text" placeholder="Es. Albert Einstein" />
        </div>

        <div class="form-actions">
          <button id="add-quote-btn" class="action-button">Aggiungi Citazione</button>
          <button id="save-quote-btn" class="action-button" style="display:none">Salva modifiche</button>
          <button id="cancel-edit-btn" class="secondary-button" style="display:none">Annulla</button>
        </div>
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

  // Toolbar handlers
  const toolbar = document.getElementById('quote-toolbar');
  const editor = document.getElementById('quote-editor');
  toolbar.addEventListener('click', (e) => {
    if (e.target.closest('button')) {
      const cmd = e.target.closest('button').dataset.cmd;
      document.execCommand(cmd, false, null);
      editor.focus();
    }

// Carica la posizione della citazione dalle impostazioni
function loadQuotePosition() {
  fetch('/api/settings')
    .then(r => r.json())
    .then(data => {
      const x = (typeof data.quotePosX === 'number') ? data.quotePosX : 35;
      const y = (typeof data.quotePosY === 'number') ? data.quotePosY : 190;
      document.getElementById('quote-pos-x').value = x;
      document.getElementById('quote-pos-y').value = y;
    })
    .catch(err => {
      const msg = document.getElementById('quote-pos-message');
      msg.textContent = `Impossibile caricare la posizione: ${err.message}`;
      msg.className = 'message error';
    });
}

// Salva la posizione della citazione nelle impostazioni
function saveQuotePosition() {
  const x = parseInt(document.getElementById('quote-pos-x').value, 10);
  const y = parseInt(document.getElementById('quote-pos-y').value, 10);
  const msg = document.getElementById('quote-pos-message');

  if (isNaN(x) || isNaN(y)) {
    msg.textContent = 'Inserisci valori numerici validi per X e Y';
    msg.className = 'message error';
    return;
  }

  fetch('/api/settings', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ quotePosX: x, quotePosY: y })
  })
  .then(r => r.json())
  .then(res => {
    if (!(res && res.success)) throw new Error((res && res.message) || 'Errore salvataggio');
    msg.textContent = 'Posizione salvata. Il dispositivo potrebbe riavviarsi per applicare le modifiche.';
    msg.className = 'message success';
  })
  .catch(err => {
    msg.textContent = `Errore: ${err.message}`;
    msg.className = 'message error';
  });
}
  });
  document.getElementById('font-size-select').addEventListener('change', (e) => {
    const size = e.target.value;
    // Applica font-size alla selezione avvolgendo in span
    document.execCommand('fontSize', false, 5); // valore segnaposto
    // Normalizza i font size 1..7 mappandoli allo span inline
    const fonts = editor.querySelectorAll('font[size="5"]');
    fonts.forEach(f => {
      const span = document.createElement('span');
      span.style.fontSize = size + 'px';
      span.innerHTML = f.innerHTML;
      f.replaceWith(span);
    });
    editor.focus();
  });

  // Stato edit corrente
  window.__editState = { active: false, category: null, index: null };

  // Event listeners azioni
  document.getElementById('add-quote-btn').addEventListener('click', addQuote);
  document.getElementById('save-quote-btn').addEventListener('click', saveEditedQuote);
  document.getElementById('cancel-edit-btn').addEventListener('click', resetForm);
  document.getElementById('save-quote-pos-btn').addEventListener('click', saveQuotePosition);

  // Carica citazioni iniziali
  loadQuotes();
  loadQuotePosition();
});

// Carica le citazioni esistenti dal server
function loadQuotes() {
  fetch('/api/quotes/all')
    .then(r => r.json())
    .then(data => {
      const payload = data && data.quotes ? data.quotes : data; // supporta entrambi i formati
      displayQuotes(payload || {});
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

  if (!quotesData || Object.keys(quotesData).length === 0) {
    container.innerHTML = '<p>Nessuna citazione trovata. Aggiungine una!</p>';
    return;
  }

  const categories = Object.keys(quotesData).sort();

  categories.forEach(category => {
    const quotes = quotesData[category];
    const categorySection = document.createElement('div');
    categorySection.className = 'quote-category';
    categorySection.innerHTML = `
      <h3>${formatCategoryName(category)} (${Array.isArray(quotes)?quotes.length:0})</h3>
      <div class="quotes-category-list">
        <table class="quotes-table">
          <thead>
            <tr>
              <th style="width:60%">Citazione</th>
              <th style="width:20%">Autore</th>
              <th style="width:20%">Azioni</th>
            </tr>
          </thead>
          <tbody id="category-${category}"></tbody>
        </table>
      </div>
    `;
    container.appendChild(categorySection);

    const tbody = categorySection.querySelector(`#category-${category}`);
    (quotes || []).forEach((quoteStr, index) => {
      const { text, author } = splitQuoteAndAuthor(quoteStr);
      const tr = document.createElement('tr');
      tr.innerHTML = `
        <td>${escapeHtml(text)}</td>
        <td>${escapeHtml(author)}</td>
        <td class="row-actions">
          <button class="edit-quote-btn" data-category="${category}" data-index="${index}">Modifica</button>
          <button class="delete-quote-btn" data-category="${category}" data-index="${index}">Elimina</button>
        </td>
      `;
      tbody.appendChild(tr);
    });
  });

  // Bind azioni
  container.querySelectorAll('.delete-quote-btn').forEach(btn => {
    btn.addEventListener('click', function() {
      deleteQuote(this.dataset.category, this.dataset.index);
    });
  });
  container.querySelectorAll('.edit-quote-btn').forEach(btn => {
    btn.addEventListener('click', function() {
      startEditQuote(this.dataset.category, parseInt(this.dataset.index, 10));
    });
  });
}

// Aggiunge una nuova citazione
function addQuote() {
  const category = document.getElementById('quote-category').value;
  const editor = document.getElementById('quote-editor');
  const author = document.getElementById('quote-author').value.trim();
  const messageElement = document.getElementById('add-message');

  const plain = editor.innerText.trim();
  if (!plain) {
    messageElement.textContent = 'Per favore inserisci una citazione.';
    messageElement.className = 'message error';
    return;
  }

  fetch('/api/quotes', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ category, quote: plain, author })
  })
  .then(r => r.json())
  .then(res => {
    if (res && res.success) {
      messageElement.textContent = 'Citazione aggiunta con successo!';
      messageElement.className = 'message success';
      resetForm();
      loadQuotes();
    } else {
      throw new Error((res && res.message) || 'Errore nell\'aggiunta della citazione');
    }
  })
  .catch(err => {
    messageElement.textContent = `Errore: ${err.message}`;
    messageElement.className = 'message error';
  });
}

// Elimina una citazione
function deleteQuote(category, index) {
  if (!confirm('Sei sicuro di voler eliminare questa citazione?')) return;
  fetch('/api/quotes/delete', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ category, index: parseInt(index, 10) })
  })
  .then(r => r.json())
  .then(res => {
    if (!(res && res.success)) throw new Error((res && res.message) || 'Errore eliminazione');
    loadQuotes();
  })
  .catch(error => alert(`Errore: ${error.message}`));
}

// Avvia modalità modifica: precompila editor e setta stato
function startEditQuote(category, index) {
  const formTitle = document.getElementById('form-title');
  const editor = document.getElementById('quote-editor');
  const authorInput = document.getElementById('quote-author');
  const catSelect = document.getElementById('quote-category');

  // Recupera il testo corrente dalla tabella
  const row = document.querySelector(`button.edit-quote-btn[data-category="${category}"][data-index="${index}"]`).closest('tr');
  const text = row.children[0].innerText;
  const author = row.children[1].innerText;

  editor.innerText = text;
  authorInput.value = author || '';
  catSelect.value = category;

  window.__editState = { active: true, category, index };
  document.getElementById('add-quote-btn').style.display = 'none';
  document.getElementById('save-quote-btn').style.display = '';
  document.getElementById('cancel-edit-btn').style.display = '';
  formTitle.textContent = 'Modifica citazione';
  editor.focus();
}

// Salva modifiche effettuando replace semplice: delete + add
function saveEditedQuote() {
  const st = window.__editState;
  if (!st.active) return;

  const category = document.getElementById('quote-category').value;
  const text = document.getElementById('quote-editor').innerText.trim();
  const author = document.getElementById('quote-author').value.trim();
  const msg = document.getElementById('add-message');

  if (!text) {
    msg.textContent = 'Il testo della citazione è obbligatorio.';
    msg.className = 'message error';
    return;
  }

  // 1) elimina la vecchia citazione nella categoria originale
  fetch('/api/quotes/delete', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ category: st.category, index: parseInt(st.index, 10) })
  })
  .then(r => r.json())
  .then(res => {
    if (!(res && res.success)) throw new Error((res && res.message) || 'Errore eliminazione');
    // 2) aggiungi la nuova (eventualmente in altra categoria)
    return fetch('/api/quotes', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ category, quote: text, author })
    });
  })
  .then(r => r.json())
  .then(res => {
    if (!(res && res.success)) throw new Error((res && res.message) || 'Errore salvataggio');
    msg.textContent = 'Citazione aggiornata';
    msg.className = 'message success';
    resetForm();
    loadQuotes();
  })
  .catch(err => {
    msg.textContent = `Errore: ${err.message}`;
    msg.className = 'message error';
  });
}

// Ripristina form in modalità aggiunta
function resetForm() {
  window.__editState = { active: false, category: null, index: null };
  document.getElementById('quote-editor').innerHTML = '';
  document.getElementById('quote-author').value = '';
  document.getElementById('quote-category').value = 'any';
  document.getElementById('add-quote-btn').style.display = '';
  document.getElementById('save-quote-btn').style.display = 'none';
  document.getElementById('cancel-edit-btn').style.display = 'none';
  document.getElementById('form-title').textContent = 'Aggiungi una nuova citazione';
}

// Formatta il nome della categoria per la visualizzazione
function formatCategoryName(category) {
  if (category === 'any') return 'Generiche';
  
  // Capitalizza la prima lettera
  return category.charAt(0).toUpperCase() + category.slice(1);
}

// Utility: separa "testo — autore" in parti
function splitQuoteAndAuthor(str) {
  const SEP = ' — ';
  const idx = str.indexOf(SEP);
  if (idx === -1) return { text: str, author: '' };
  return { text: str.substring(0, idx), author: str.substring(idx + SEP.length) };
}

// Escape per prevenire injection nel rendering tabellare
function escapeHtml(s) {
  return String(s || '')
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#039;');
}
