const char labstaff_css[] PROGMEM = R"rawliteral(
:root {
  --bg-dark: url("dark-bg.png");
  --bg-light: url("light-bg.png");
  --text-light: #fff;
  --text-dark: #fff;
  --table-dark: rgba(0, 0, 0, 0.6);
  --table-light: rgba(255, 255, 255, 0.85);
  --border-dark: #444;
  --border-light: #aaa;
}

body {
  font-family: Arial, sans-serif;
  background-image: var(--bg-dark);
  background-size: cover;
  margin: 0;
  padding: 0;
  color: var(--text-light);
  transition: background-image 0.3s, color 0.3s;
}

.container {
  max-width: 1000px;
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

table {
  width: 100%;
  border-collapse: collapse;
  background: transparent;
}

th, td {
  border: 1px solid var(--border-dark);
  padding: 8px 12px;
  text-align: left;
}

th {
  background-color: rgba(30, 30, 30, 0.8);
}

td pre {
  white-space: pre-wrap;
  word-wrap: break-word;
}

button {
  padding: 6px 10px;
  margin: 2px;
  border: none;
  border-radius: 4px;
  cursor: pointer;
  font-size: 0.9em;
}

.accept {
  background-color: #4CAF50;
  color: white;
}

.reject {
  background-color: #f44336;
  color: white;
}

.footer {
  margin-top: 30px;
  text-align: center;
  font-size: 0.9em;
  color: #bbb;
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

.grayed-out {
  opacity: 0.5;
}

.highlight-box {
  background-color: #fff7a1;
  color: #000;
  padding: 15px;
  border-radius: 8px;
  margin-top: 15px;
}
)rawliteral";
