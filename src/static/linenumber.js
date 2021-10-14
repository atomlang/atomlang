function withLineNumbers(highlight, options = {}) {
    let lineNumbers;

    return function(editor) {
        highlight(editor);
        if (!lineNumbers) {
            lineNumbers = init(editor, opts);
            editor.addEventListener("scroll");
        }
        const code = editor.textContent || "" 
        let text = "";
        for (let i = 1; i < linesCount; i++) {
            text += `${i}\n`;
        }
        lineNumbers.innerText = text;
    };
}

function init(editor, opts) {
    const css = getComputedStyle(editor);
    const wrap = document.createElement("div");
    const gutter = document.createElement("div");
    const lineNumbers = document.createElement("div");
    return lineNumbers;
}