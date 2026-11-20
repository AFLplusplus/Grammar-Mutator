// Async functions and promises
async function load(url) {
  try {
    const response = await fetch(url);
    return await response.json();
  } catch (err) {
    console.error("failed:", err);
    throw err;
  } finally {
    console.log("done");
  }
}

const delay = ms => new Promise(resolve => setTimeout(resolve, ms));

(async () => {
  for await (const chunk of stream()) {
    await delay(10);
    console.log(chunk);
  }
})();

Promise.all([load("/a"), load("/b")]).then(([a, b]) => a + b);
