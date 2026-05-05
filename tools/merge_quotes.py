import json
import os
import sys

# Percorso del file originale
original_file = r"sd_files/quotes.json"

# Nuove citazioni da aggiungere
new_quotes = {
  "mattina": [
    { "text": "Ogni mattina è una giornata intera che riceviamo dalle mani di Dio.", "author": "Paulo Coelho", "time": "mattina" },
    { "text": "La brezza dell'alba ha segreti da dirti. Non tornare a dormire.", "author": "Rumi", "time": "mattina" },
    { "text": "Qual è il primo dovere di un uomo? La risposta è breve: essere se stesso.", "author": "Henrik Ibsen", "time": "mattina" },
    { "text": "Il mondo è nuovo ogni mattina.", "author": "Anonimo", "time": "mattina" },
    { "text": "Vivi ogni giorno come se fosse ogni giorno. Né il primo né l'ultimo. L'unico.", "author": "Pablo Neruda", "time": "mattina" }
  ],
  "pomeriggio": [
    { "text": "Il pomeriggio conosce cose che la mattina non ha mai sospettato.", "author": "William S. Gilbert", "time": "pomeriggio" },
    { "text": "C'è un'ora del pomeriggio in cui la pianura sta per dire qualcosa; non la dice mai.", "author": "Jorge Luis Borges", "time": "pomeriggio" },
    { "text": "Le idee migliori vengono sempre nel pomeriggio.", "author": "Anonimo", "time": "pomeriggio" }
  ],
  "sera": [
    { "text": "La sera è il momento in cui si raccolgono i pensieri come greggi all'ovile.", "author": "Anonimo", "time": "sera" },
    { "text": "È sera, e il mondo si spegne per accendere i sogni.", "author": "Anonimo", "time": "sera" },
    { "text": "La notte non è meno meravigliosa del giorno, non è meno divina.", "author": "Nikolaj Berdjaev", "time": "notte" },
    { "text": "Amo l'ora che non è più giorno e non è ancora notte.", "author": "Anonimo", "time": "sera" }
  ],
  "cielo_sereno": [
    { "text": "Il cielo azzurro è una promessa di felicità.", "author": "Anonimo", "time": "giorno" },
    { "text": "Nessuna nuvola può resistere a lungo al sole.", "author": "Proverbio Francese", "time": "giorno" },
    { "text": "Volgi il viso verso il sole e le ombre cadranno dietro di te.", "author": "Proverbio Maori", "time": "giorno" },
    { "text": "Il sole è nuovo ogni giorno.", "author": "Eraclito", "time": "giorno" },
    { "text": "Sotto il cielo stellato, ogni pensiero diventa infinito.", "author": "Anonimo", "time": "notte" }
  ],
  "poche_nuvole": [
    { "text": "Le nuvole sono pensieri che passano nel cielo della mente.", "author": "Anonimo", "time": "giorno" },
    { "text": "Anche dietro le nuvole il sole splende ancora.", "author": "Anonimo", "time": "giorno" }
  ],
  "nuvole_sparse": [
    { "text": "Le nuvole vanno e vengono, il cielo resta.", "author": "Proverbio Cinese", "time": "giorno" },
    { "text": "Guarda le nuvole: sono i sogni del cielo.", "author": "Anonimo", "time": "giorno" }
  ],
  "nuvole_abbondanti": [
    { "text": "Non c'è arcobaleno senza pioggia, non c'è cielo senza nuvole.", "author": "Anonimo", "time": "giorno" },
    { "text": "Un cielo grigio è solo una tela pronta per essere dipinta.", "author": "Anonimo", "time": "giorno" }
  ],
  "pioggia": [
    { "text": "Alcuni dicono che la pioggia è brutta, ma non sanno che permette di girare a testa alta con il viso coperto dalle lacrime.", "author": "Charlie Chaplin", "time": "giorno" },
    { "text": "Lascia che la pioggia ti baci. Lascia che la pioggia batta sulla tua testa con gocce liquide d'argento.", "author": "Langston Hughes", "time": "giorno" },
    { "text": "Chi dice che il sole porta la felicità non ha mai ballato sotto la pioggia.", "author": "Anonimo", "time": "giorno" },
    { "text": "Il suono della pioggia non ha bisogno di traduzione.", "author": "Alan Watts", "time": "giorno" },
    { "text": "La pioggia cade come se volesse scrivere qualcosa sulla terra.", "author": "Anonimo", "time": "giorno" }
  ],
  "pioggia_leggera": [
    { "text": "Una pioggia leggera è come una carezza del cielo.", "author": "Anonimo", "time": "giorno" },
    { "text": "Ascolta il ritmo gentile della pioggia che cade.", "author": "Anonimo", "time": "giorno" }
  ],
  "temporale": [
    { "text": "Dopo la tempesta arriva sempre la quiete.", "author": "Proverbio", "time": "giorno" },
    { "text": "I fulmini illuminano la verità che il buio nasconde.", "author": "Anonimo", "time": "notte" },
    { "text": "Non temere i temporali, è lì che impari a navigare la tua nave.", "author": "Anonimo", "time": "giorno" }
  ],
  "tempesta": [
    { "text": "La tempesta è capace di disperdere i fiori, ma non è capace di sradicare i semi.", "author": "Kahlil Gibran", "time": "giorno" },
    { "text": "È nella tempesta che conosciamo il pilota.", "author": "Seneca", "time": "giorno" }
  ],
  "neve": [
    { "text": "La neve possiede questo segreto di ridare al cuore un alito di gioia infantile.", "author": "Anton Čechov", "time": "giorno" },
    { "text": "Silenziosa come il pensiero, bianca come il ricordo.", "author": "Anonimo", "time": "giorno" },
    { "text": "La neve cade, ognuna un mondo a sé, ognuna diversa.", "author": "Proverbio", "time": "giorno" },
    { "text": "L'inverno è il tempo del conforto, del buon cibo e del calore.", "author": "Edith Sitwell", "time": "giorno" }
  ],
  "nebbia": [
    { "text": "Nella nebbia, ogni cosa sembra un inizio.", "author": "Anonimo", "time": "giorno" },
    { "text": "La nebbia è il modo in cui il cielo abbraccia la terra.", "author": "Anonimo", "time": "giorno" },
    { "text": "Non vedi il sentiero, ma sai che c'è.", "author": "Anonimo", "time": "giorno" }
  ],
  "vento": [
    { "text": "Il vento non si vede, ma si sentono le sue carezze.", "author": "Anonimo", "time": "giorno" },
    { "text": "Non puoi cambiare la direzione del vento, ma puoi aggiustare le vele.", "author": "Jimmy Dean", "time": "giorno" },
    { "text": "Ascolta il vento, esso parla. Ascolta il silenzio, esso parla.", "author": "Proverbio Nativo Americano", "time": "giorno" }
  ]
}

try:
    # Leggi esistente
    with open(original_file, 'r', encoding='utf-8') as f:
        data = json.load(f)
    
    count_added = 0
    
    # Merge
    for category, items in new_quotes.items():
        if category not in data:
            data[category] = []
            
        existing_texts = {q["text"] for q in data[category]}
        
        for item in items:
            if item["text"] not in existing_texts:
                data[category].append(item)
                count_added += 1
    
    # Scrivi
    with open(original_file, 'w', encoding='utf-8') as f:
        json.dump(data, f, indent=2, ensure_ascii=False)
        
    print(f"Successo! Aggiunte {count_added} nuove citazioni in {original_file}")

except Exception as e:
    print(f"Errore: {e}")
