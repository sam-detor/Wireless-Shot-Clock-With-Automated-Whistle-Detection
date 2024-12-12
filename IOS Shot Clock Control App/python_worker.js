//Chat gpt generated
self.onmessage = async (event) => {
    if (event.data.type === "start") {
      // Load Pyodide
      importScripts("https://cdn.jsdelivr.net/pyodide/v0.23.4/full/pyodide.js");
      const pyodide = await loadPyodide();
  
      // Define and start the Python script
      try {
        const pythonScript = `
  import time
  while True:
      print("Running indefinitely...")
      time.sleep(5)
        `;
        await pyodide.runPythonAsync(pythonScript);
      } catch (err) {
        self.postMessage({ type: "error", message: err.toString() });
      }
    }
  };