# ESP32-2432S028R-duino-tft-dimming-Clock-alarm-TICKER
![WhatsApp Image 2026-02-21 at 23 28 53 (1)](https://github.com/user-attachments/assets/4d22968e-339b-4bbc-b1c2-7ad389873eda)
![WhatsApp Image 2026-02-21 at 23 28 53](https://github.com/user-attachments/assets/9caaf293-7912-4e44-ac60-8220a28916b3)
![WhatsApp Image 2026-03-06 at 18 38 05 (2)](https://github.com/user-attachments/assets/f2e30949-3026-4498-959e-4c38ad675785)
![WhatsApp Image 2026-03-06 at 18 38 05 (1)](https://github.com/user-attachments/assets/e3c75d46-81dc-4739-b924-1c37c14b3e40)
![WhatsApp Image 2026-03-06 at 18 38 05](https://github.com/user-attachments/assets/5048aa7c-1cb0-460c-920f-14522d7991bd)


# TFT-Duino-coin-wallet-monitor-esp32-2432s028r-CYD-Real-time-clock-alarm
![597755445_25373566025667513_3922752145654390881_n](https://github.com/user-attachments/assets/ea333a98-bd17-46e8-989f-f59e8e1e6cf8)
![597850397_25373566009000848_1899672088036288368_n](https://github.com/user-attachments/assets/3f268631-8219-4078-8a07-28179cd52456)
![Imagine WhatsApp 2025-11-02 la 13 00 24_d663f3b1](https://github.com/user-attachments/assets/f83b96ce-1bcf-429f-a82c-a170e8d6e3bb)
![Imagine WhatsApp 2025-11-02 la 12 58 09_1f3f1c5d](https://github.com/user-attachments/assets/ed943ca2-d09a-44f9-9753-18dcf4a0cba0)
![IMG-20251102-WA0012](https://github.com/user-attachments/assets/112aaa05-0616-4db5-bac1-43e2fdb42176)
![IMG-20251102-WA0011](https://github.com/user-attachments/assets/3830ab1b-fd5d-43c2-bf19-c946e89e1be4)
![IMG-20251102-WA0010](https://github.com/user-attachments/assets/ecf83a1e-8688-40f9-889d-dd2e6259f2fb)
![IMG-20251102-WA0009](https://github.com/user-attachments/assets/8466130d-9c21-4aa0-8e4d-646e1b6cc171)
![IMG-20251102-WA0008](https://github.com/user-attachments/assets/ff67d8d0-54fd-4cfe-adf9-b4cd50e54b4a)
![IMG-20251102-WA0007](https://github.com/user-attachments/assets/ba73b644-2671-4b61-9c21-691b6bd3f704)
![IMG-20251102-WA0006](https://github.com/user-attachments/assets/a28cfa92-6b12-46fd-891c-2e11c4c69efc)
![IMG-20251102-WA0005](https://github.com/user-attachments/assets/ed2445fd-7186-4895-a082-fb60bb89299b)
![IMG-20251102-WA0004](https://github.com/user-attachments/assets/f2d90ab7-70c4-4e9b-86d0-24466ed9f7b6)
![IMG-20251102-WA0003](https://github.com/user-attachments/assets/58bcd866-7bd9-43a7-a2b2-43ca4c99a29e)
![IMG-20251102-WA0002](https://github.com/user-attachments/assets/d6532554-02fe-4ee9-8472-9812c9de6dd7)
![IMG-20251102-WA0001](https://github.com/user-attachments/assets/87715ab9-95dc-4d19-9381-73cfa0649711)
![Imagine WhatsApp 2025-11-02 la 14 26 32_5203d9f4](https://github.com/user-attachments/assets/e0e56306-9e69-4377-8f2d-fa09583dd386) Piezo

Prezentare:
---

### **Scop general**

Un cod complet și destul de complex pentru ESP32, care combină multe componente: ecran TFT tactil, WiFi, NTP, alarme, control de volum, interfață grafică, conexiune la API-ul Duino-Coin, EEPROM etc.
Hai să-ți explic pe scurt ce face acest cod și cum e structurat, pas cu pas 👇

🧩 1. Biblioteci incluse
Codul include multe librării:
WiFi.h, HTTPClient.h – pentru conectarea și comunicarea prin internet;
ArduinoJson.h – pentru a interpreta datele JSON primite de la serverul Duino-Coin;
Adafruit_GFX.h, Adafruit_ILI9341.h – pentru controlul ecranului TFT 320x240;
XPT2046_Touchscreen.h – pentru controlul ecranului tactil;
EEPROM.h – pentru salvarea setărilor (ex: calibrare touch, volum, alarmă);
time.h – pentru sincronizarea orei prin NTP;
driver/ledc.h – pentru PWM hardware (folosit la reglarea luminozității ecranului);
Arduino.h – biblioteca de bază a mediului Arduino/ESP32.

📶 2. Conectarea WiFi și Duino-Coin
Se definește utilizatorul Duino-Coin (ducoUser).
WiFiMulti permite conectarea la mai multe rețele dacă sunt memorate.
Codul folosește API-urile Duino-Coin:
/balances/{user} – pentru sold;
/users/{user} – pentru informații despre mineri.

## De reținut
Pentru monitorizare personală: O interogare la fiecare 30 - 60 (” if (currentMillis - lastApiCheck > 30000)” ) de secunde este considerată ideală. Aceasta oferă date actualizate fără a solicita inutil serverele proiectului.
Frecvența maximă sigură: Evitați să faceți mai mult de o solicitare la 5-10 secunde. Solicitările mai dese pot fi interpretate ca un atac de tip spam sau bot. 
Recomand setarea timpului de reîmprospătare al panoului de control al actualizărilor la 120.000 de milisecunde, adică 2 minute. NU deschideți mai multe dispozitive simultan cu aceeași aplicație, deoarece generează un conflict. Cloudfare: Prea multe solicitări pe minut.

🌞 3. Controlul luminozității și LDR
LDR-ul pe pinul 34 măsoară lumina ambientală.
PWM-ul de pe pinul 21 controlează luminozitatea ecranului în funcție de lumină.
Se face o “netezire” a valorilor (smoothBrightness) pentru a evita variațiile bruște.

🔊 4. Volum, alarmă și DAC audio
Se stochează volumul în EEPROM (ADDR_ALARM_VOL).
Buzzerul este pe pinul DAC (26) — se generează un sunet analogic (dacWrite).
Funcția beepDAC(freq, duration) produce un ton cu frecvența dorită și durata specificată.
Exista controale grafice pentru volum (“V+” și “V−”).

🕒 5. Ceasul și sincronizarea NTP
Se conectează la pool.ntp.org pentru ora exactă.
Se afișează ora locală mare pe ecran, în format HH:MM:SS.
Timpul e menținut precis folosind diferența de millis() de la ultima sincronizare.

⏰ 6. Alarma
Setări: oră, minut, volum, activare/dezactivare;
Se salvează în EEPROM;
La ora setată, funcția checkAlarm() pornește alarma (startAlarm()), care afișează “ALARM!” pe ecran și redă un ton prin DAC.

📱 7. Ecranul tactil (Touch)
Se calibrează la prima utilizare (valorile minX/maxX etc.);
Coordonatele sunt mapate la ecranul de 320x240 pixeli;
Se definesc butoane tactile:
Buton principal (“PAY-15'”/“GiftPay”);
Schimbare pagină (wallet/miners);
Configurare WiFi;
Buton alarmă;
Butoane volum.
Beep la atingerea tastelor, butoanelor si display.

🧮 8. Pagini interfață:
Pagina 1: Ceas, alarmă, volum;
Pagina 2: Minerii activi Duino-Coin (API call);
Pagina 3: Lista rețelelor WiFi disponibile + conectare cu tastatură virtuală.

🔤 9. Tastatura virtuală
Este complet grafică, desenată pe TFT;
Are litere, cifre, simboluri, spațiu, caps lock, OK și EXIT;
Returnează parola introdusă pentru conexiunea WiFi;
Caps lock și tastele au efect vizual (feedback tactil).

💾 10. EEPROM
Stochează permanent:
calibrarea ecranului tactil (min/max X/Y);
volumul alarmei;
ora/minutul alarmei;
starea activă (ON/OFF) a alarmei.

🎨 11. Animația de pornire “Duino-Coin”
Efect “fade-in” și “fade-out” al backlight-ului;
Cercuri concentrice și text animat “DUINO-COIN”;
Text “By Sor Adi”.

🌐 12. Interacțiuni cu utilizatorul (verificaTouch)
Funcția verificaTouch() gestionează TOT ce atingi pe ecran:
oprește alarma dacă sună;
schimbă paginile;
reglează volumul;
deschide tastatura pentru setarea alarmei;
deschide lista WiFi și permite conectarea.

🔔 13. Melodie “Super Mario” (începutul)
La finalul codului (trunchiat în mesajul tău), este pregătită o listă de frecvențe NOTE_C1, NOTE_D1, etc.
Probabil urmează o funcție playMario() care redă melodia “Super Mario Theme” prin buzzerul DAC.

🧠 Rezumat scurt:
Acesta este un proiect complex de ceas inteligent cu ecran tactil și conectivitate, pentru ESP32, care:
afișează ora reală sincronizată NTP;
redă o alarmă configurabilă;
controlează volumul;
se conectează la WiFi printr-o interfață grafică complet tactilă;
se conectează la API-ul Duino-Coin pentru a afișa informații despre sold și mineri;
memorează setările în EEPROM.
Mulțumesc și celor de la Duino-Coin pentru faptul că au pornit https://duinocoin.com/
---
Autor: Sorinescu Adrian.
https://www.facebook.com/groups/454273464099316
