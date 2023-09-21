let currentLine = 0;
let textbox = document.getElementById("text")
let image = document.querySelector("img")

window.addEventListener("load", (e) => {
    getLine(currentLine).then(function(result) {
        textbox.innerText = result;
    });
});

document.getElementById("next-btn").addEventListener("click", nextLine);

function nextLine(e) {
    getLine(currentLine++).then(function(result) {
        if(!result) return currentLine--;
        let should_skip = parseLine(result);
        if(should_skip) {
            nextLine(e);
        } else {
            textbox.innerText = result;
        }
    });
}

function parseLine(line) {
    if(line.includes("@src")) {
        image.src = line.slice(5);
        return true;
    } else {
        return false;
    }
}

async function getLine(_line) {
    try {
        let response = await fetch('/get-line?' + new URLSearchParams({
            line: _line,
        }));
        if(response.status != 200) {
            throw new Error("Bad request");
        }
        let data = await response.text();
        return data;
    } catch(error) {
        return false;
    }
}
