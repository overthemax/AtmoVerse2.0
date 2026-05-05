// Navbar persistente per AtmoVerse (senza pulsanti che modificano la visualizzazione della pagina)
(function(){
  try {
    const nav = document.createElement('div');
    nav.className = 'navbar';
    nav.innerHTML = `
      <a class="nav-item" data-path="/" href="/" aria-label="Home" title="Home">
        <svg class="home-icon" width="20" height="20" viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg" aria-hidden="true">
          <path d="M3 10.5L12 3l9 7.5V20a1 1 0 0 1-1 1h-5v-6H9v6H4a1 1 0 0 1-1-1v-9.5z" stroke="#6b5940" stroke-width="1.5" fill="none"/>
        </svg>
      </a>
      <a class="nav-item" data-path="/quotes.html" href="/quotes.html">Citazioni</a>
      <a class="nav-item" data-path="/settings.html" href="/settings.html">Impostazioni</a>
      <a class="nav-item" data-path="/personalizzazione.html" href="/personalizzazione.html">Personalizzazione</a>
      <a class="nav-item" data-path="/info.html" href="/info.html">Info</a>
      <a class="nav-item" data-path="/diagnostics.html" href="/diagnostics.html">Diagnostica</a>
    `;
    // Inserisci come primo elemento del body
    const body = document.body;
    if (body.firstChild) body.insertBefore(nav, body.firstChild);
    else body.appendChild(nav);

    // Evidenzia voce attiva
    const path = location.pathname === '/' ? '/' : location.pathname;
    const items = nav.querySelectorAll('.nav-item');
    items.forEach(a => {
      const p = a.getAttribute('data-path');
      if (p === path) a.classList.add('active');
    });

    // Nessun pulsante o logica che modifichi la visualizzazione del sito.
  } catch(e) { /* no-op */ }
})();
