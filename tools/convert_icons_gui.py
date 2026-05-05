import os
import sys
import tkinter as tk
from tkinter import filedialog, messagebox, ttk
from PIL import Image, ImageOps
import time

# Tentativo di importare librerie per SVG
try:
    from svglib.svglib import svg2rlg
    from reportlab.graphics import renderPM
    SVG_SUPPORT = True
except ImportError:
    SVG_SUPPORT = False

class IconConverterApp:
    def __init__(self, root):
        self.root = root
        self.root.title("AtmoVerse Icon Converter (SVG/PNG -> BMP)")
        self.root.geometry("600x500")
        self.root.resizable(False, False)

        # Stile
        style = ttk.Style()
        style.configure("TButton", padding=6, relief="flat", background="#ccc")
        style.configure("TLabel", font=("Segoe UI", 10))

        # Variabili
        self.src_dir = tk.StringVar()
        self.dest_dir = tk.StringVar()
        self.icon_size = tk.IntVar(value=100)
        self.invert_colors = tk.BooleanVar(value=False)
        self.dithering = tk.BooleanVar(value=False)
        self.threshold = tk.IntVar(value=128)

        # UI Layout
        main_frame = ttk.Frame(root, padding="20")
        main_frame.pack(fill=tk.BOTH, expand=True)

        # Titolo
        ttk.Label(main_frame, text="Convertitore Icone per E-Ink", font=("Segoe UI", 16, "bold")).pack(pady=(0, 20))

        if not SVG_SUPPORT:
            warn_frame = ttk.Frame(main_frame, style="TFrame")
            warn_frame.pack(fill=tk.X, pady=(0, 10))
            ttk.Label(warn_frame, text="⚠️ Librerie SVG mancanti (svglib, reportlab).", foreground="red").pack()
            ttk.Label(warn_frame, text="Installa con: pip install svglib reportlab", foreground="blue").pack()

        # Cartella Sorgente
        src_frame = ttk.LabelFrame(main_frame, text="Cartella Sorgente (SVG/PNG)", padding="10")
        src_frame.pack(fill=tk.X, pady=5)
        
        ttk.Entry(src_frame, textvariable=self.src_dir).pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(0, 5))
        ttk.Button(src_frame, text="Sfoglia...", command=self.browse_src).pack(side=tk.RIGHT)

        # Cartella Destinazione
        dest_frame = ttk.LabelFrame(main_frame, text="Cartella Destinazione (BMP)", padding="10")
        dest_frame.pack(fill=tk.X, pady=5)
        
        ttk.Entry(dest_frame, textvariable=self.dest_dir).pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(0, 5))
        ttk.Button(dest_frame, text="Sfoglia...", command=self.browse_dest).pack(side=tk.RIGHT)

        # Opzioni
        opt_frame = ttk.LabelFrame(main_frame, text="Opzioni Conversione", padding="10")
        opt_frame.pack(fill=tk.X, pady=10)

        # Size
        size_frame = ttk.Frame(opt_frame)
        size_frame.pack(fill=tk.X, pady=5)
        ttk.Label(size_frame, text="Dimensione (px):").pack(side=tk.LEFT)
        ttk.Entry(size_frame, textvariable=self.icon_size, width=10).pack(side=tk.LEFT, padx=10)

        # Checkboxes
        ttk.Checkbutton(opt_frame, text="Inverti Colori (Bianco <-> Nero)", variable=self.invert_colors).pack(anchor="w", pady=2)
        ttk.Checkbutton(opt_frame, text="Applica Dithering (per sfumature)", variable=self.dithering).pack(anchor="w", pady=2)
        
        # Threshold
        thresh_frame = ttk.Frame(opt_frame)
        thresh_frame.pack(fill=tk.X, pady=5)
        ttk.Label(thresh_frame, text="Soglia B/N (0-255):").pack(side=tk.LEFT)
        thresh_scale = ttk.Scale(thresh_frame, from_=0, to=255, variable=self.threshold, orient=tk.HORIZONTAL)
        thresh_scale.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=10)
        ttk.Label(thresh_frame, textvariable=self.threshold).pack(side=tk.LEFT)

        # Log
        self.log_text = tk.Text(main_frame, height=8, width=50, font=("Consolas", 9))
        self.log_text.pack(fill=tk.BOTH, expand=True, pady=10)

        # Bottone Converti
        ttk.Button(main_frame, text="AVVIA CONVERSIONE", command=self.convert_icons).pack(pady=10, fill=tk.X)

    def log(self, msg):
        self.log_text.insert(tk.END, msg + "\n")
        self.log_text.see(tk.END)
        self.root.update()

    def browse_src(self):
        d = filedialog.askdirectory()
        if d: self.src_dir.set(d)

    def browse_dest(self):
        d = filedialog.askdirectory()
        if d: self.dest_dir.set(d)

    def convert_icons(self):
        src = self.src_dir.get()
        dest = self.dest_dir.get()
        size = self.icon_size.get()

        if not src or not dest:
            messagebox.showerror("Errore", "Seleziona le cartelle sorgente e destinazione")
            return

        if not os.path.exists(dest):
            os.makedirs(dest)

        self.log(f"Inizio conversione...")
        self.log(f"Sorgente: {src}")
        self.log(f"Destinazione: {dest}")
        self.log(f"Target Size: {size}x{size}")

        count = 0
        errors = 0

        for filename in os.listdir(src):
            if filename.lower().endswith(('.svg', '.png', '.jpg', '.jpeg')):
                filepath = os.path.join(src, filename)
                name, ext = os.path.splitext(filename)
                dest_path = os.path.join(dest, name + ".bmp")

                try:
                    img = None
                    
                    # Gestione SVG
                    if filename.lower().endswith('.svg'):
                        if SVG_SUPPORT:
                            # Converti SVG in Drawing ReportLab
                            drawing = svg2rlg(filepath)
                            # Renderizza in immagine PIL (via PNG in memoria)
                            from io import BytesIO
                            png_data = BytesIO()
                            renderPM.drawToFile(drawing, png_data, fmt="PNG")
                            png_data.seek(0)
                            img = Image.open(png_data)
                        else:
                            self.log(f"SKIP: {filename} (Librerie SVG non installate)")
                            continue
                    else:
                        # Immagini standard
                        img = Image.open(filepath)

                    if img:
                        # Resize
                        img = img.resize((size, size), Image.Resampling.LANCZOS)

                        # Converti in Scala di Grigi
                        gray = img.convert('L')

                        # Inverti se richiesto
                        if self.invert_colors.get():
                            gray = ImageOps.invert(gray)

                        # Converti in 1-bit (Bianco e Nero)
                        if self.dithering.get():
                            bw = gray.convert('1') # Default dithering
                        else:
                            # Thresholding manuale
                            thresh = self.threshold.get()
                            bw = gray.point(lambda x: 0 if x < thresh else 255, '1')

                        # Salva come BMP
                        bw.save(dest_path, "BMP")
                        self.log(f"OK: {filename} -> {name}.bmp")
                        count += 1

                except Exception as e:
                    self.log(f"ERR: {filename} - {str(e)}")
                    errors += 1

        self.log(f"-----")
        self.log(f"Completato! Convertiti: {count}, Errori: {errors}")
        messagebox.showinfo("Finito", f"Conversione completata.\nFile creati: {count}")

if __name__ == "__main__":
    root = tk.Tk()
    app = IconConverterApp(root)
    root.mainloop()
