const char borrowform_css[] PROGMEM = R"rawliteral(
:root {
  --bg-dark: url("dark-bg.png");
  --bg-light: url("light-bg.png");
  --text-light: #fff;
  --text-dark: #fff;
}

body {
  font-family: Arial, sans-serif;
  background-image: var(--bg-dark);
  background-size: cover;
  margin: 0;
  padding: 0;
  color: var(--text-dark);
  transition: background-image 0.3s ease, color 0.3s ease;
}

.container {
  max-width: 720px;
  margin: 40px auto;
  background-color: rgba(0, 0, 0, 0.6);
  padding: 20px;
  border-radius: 10px;
  backdrop-filter: blur(6px);
}

h2 {
  text-align: center;
  margin-bottom: 25px;
}

label {
  display: block;
  margin-top: 10px;
  margin-bottom: 5px;
}

input, textarea {
  width: 100%;
  padding: 10px;
  border-radius: 5px;
  border: none;
  background-color: #333;
  color: #fff;
  margin-bottom: 10px;
}

.scrollable {
  max-height: 200px;
  overflow-y: auto;
  margin-top: 10px;
}

button {
  background-color: #2196F3;
  color: white;
  border: none;
  padding: 6px 10px;
  border-radius: 4px;
  cursor: pointer;
  transition: background-color 0.2s;
  font-size: 0.9em;
}

button:hover {
  background-color: #1e88e5;
}

.flat-button {
  display: block;
  margin: 30px auto 0 auto;
  padding: 10px 20px;
  font-size: 1em;
}

.toggle-theme {
  position: absolute;
  top: 20px;
  right: 20px;
  font-size: 0.9em;
  padding: 5px 10px;
  background-color: #444;
  border: none;
  color: white;
  border-radius: 4px;
  cursor: pointer;
}

.uid-box {
  background: #333;
  padding: 10px;
  margin-top: 15px;
  text-align: center;
  font-weight: bold;
  font-size: 1.2em;
  display: none;
}

#confirmButton {
  display: none;
  margin-top: 15px;
  background-color: #4CAF50;
}

.footer {
  margin-top: 30px;
  text-align: center;
  font-size: 0.9em;
  color: #bbb;
}

.item-row {
  display: flex;
  gap: 10px;
  margin-bottom: 10px;
  align-items: center;
}

.item-row input[type="number"] {
  max-width: 80px;
}

.item-row button {
  background-color: #f44336;
  padding: 4px 8px;
  font-size: 0.8em;
}

.controls {
  text-align: right;
  margin-top: 10px;
}
)rawliteral";