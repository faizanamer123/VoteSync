let clientId = "";
let secondsLeft = 0;
let statusInterval = null;
let voteSubmitted = false;
let countdownInterval = null;
let statsInterval = null;

const COLORS = ["#4CAF50", "#2196F3", "#FF9800", "#9C27B0", "#f44336"];

// Connect client to server and get a client ID
async function connectClient() {
    const res = await fetch('http://127.0.0.1:8080/connect');
    clientId = await res.text();
    console.log("Connected as:", clientId);
}

// Check the current voting status from server
async function checkStatus() {
    const res = await fetch('http://127.0.0.1:8080/status');
    const status = await res.text();

    if (status.startsWith('waiting')) {
        if (!voteSubmitted) showScreen('waitingScreen');
    }
    else if (status.startsWith('voting')) {
        if (!voteSubmitted) showScreen('votingScreen');
    }
    else if (status.startsWith('ended')) {
        clearInterval(statusInterval);
        clearInterval(statsInterval);
        showScreen('resultsScreen');
        getResults();
    }
}

// Submit the selected vote to the server
async function submitVote(event) {
    event.preventDefault();
    if (!clientId) {
        alert("No client ID assigned. Please refresh page.");
        return;
    }

    const selected = document.querySelector('input[name="candidate"]:checked').value;
    await fetch('http://127.0.0.1:8080/vote', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: clientId + '&candidate=' + encodeURIComponent(selected)
    });

    voteSubmitted = true;
    showScreen('submittedScreen');
    startCountdown();
    startFetchingStats();
}

// Start countdown timer until voting ends
function startCountdown() {
    countdownInterval = setInterval(async () => {
        const res = await fetch('http://127.0.0.1:8080/status');
        const status = await res.text();
        if (status.startsWith('voting')) {
            const parts = status.split('|');
            secondsLeft = parseInt(parts[1]);
            let minutes = Math.floor(secondsLeft / 60);
            let seconds = secondsLeft % 60;
            document.getElementById('countdownTimer').textContent =
                `Voting ends in ${String(minutes).padStart(2, '0')}:${String(seconds).padStart(2, '0')}`;
        }
        else if (status.startsWith('ended')) {
            clearInterval(countdownInterval);
            clearInterval(statsInterval);
            showScreen('resultsScreen');
            getResults();
        }
    }, 1000);
}

// Fetch final voting results
async function getResults() {
    const res = await fetch('http://127.0.0.1:8080/results');
    const data = await res.json();

    document.getElementById('winnerName').textContent = "Winner: " + data.winner;

    const chartDiv = document.getElementById('resultsChart');
    chartDiv.innerHTML = "";

    let colorIndex = 0;
    for (const [candidate, votes] of Object.entries(data.votes)) {
        const bar = document.createElement('div');
        bar.style.background = COLORS[colorIndex % COLORS.length];
        bar.style.height = '30px';
        bar.style.width = (votes * 40) + 'px';
        bar.style.margin = '10px';
        bar.style.color = 'white';
        bar.style.padding = '5px';
        bar.style.borderRadius = '8px';
        bar.textContent = `${candidate} (${votes})`;
        chartDiv.appendChild(bar);
        colorIndex++;
    }
}

// Fetch live voting statistics during voting
async function fetchStats() {
    const res = await fetch('http://127.0.0.1:8080/stats');
    const data = await res.json();

    const chartDiv = document.getElementById('statsChart');
    if (!chartDiv) return;

    chartDiv.innerHTML = "";

    let colorIndex = 0;
    for (const [candidate, votes] of Object.entries(data.votes)) {
        const bar = document.createElement('div');
        bar.style.background = COLORS[colorIndex % COLORS.length];
        bar.style.height = '20px';
        bar.style.width = (votes * 30) + 'px';
        bar.style.margin = '5px';
        bar.style.color = 'white';
        bar.style.padding = '5px';
        bar.style.borderRadius = '8px';
        bar.textContent = `${candidate}: ${votes}`;
        chartDiv.appendChild(bar);
        colorIndex++;
    }
}

// Start polling for live stats
function startFetchingStats() {
    statsInterval = setInterval(fetchStats, 3000);
}

// Properly hide all screens except the one needed
function showScreen(id) {
    document.querySelectorAll('.screen').forEach(div => {
        if (div.id === id) {
            div.classList.remove('hidden');
        } else {
            div.classList.add('hidden');
        }
    });
}

// Setup vote form
document.getElementById('voteForm').addEventListener('submit', submitVote);

// Start the connection and status checking
connectClient();
statusInterval = setInterval(checkStatus, 3000);


//let clientId = "";
//let secondsLeft = 0;
//let statusInterval = null;
//let voteSubmitted = false;
//let countdownInterval = null;
//let statsInterval = null;

//const COLORS = ["#4CAF50", "#2196F3", "#FF9800", "#9C27B0", "#f44336"];

//async function connectClient() {
//    const res = await fetch('http://127.0.0.1:8080/connect');
//    clientId = await res.text();
//    console.log("Connected with client ID:", clientId);
//}

//async function checkStatus() {
//    const res = await fetch('http://127.0.0.1:8080/status');
//    const status = await res.text();

//    if (voteSubmitted) return; // if vote submitted, don't switch screens anymore

//    if (status.startsWith('waiting')) {
//        showScreen('waitingScreen');
//    }
//    else if (status.startsWith('voting')) {
//        showScreen('votingScreen');
//    }
//    else if (status.startsWith('ended')) {
//        clearInterval(statusInterval);
//        clearInterval(statsInterval);
//        getResults();
//    }
//}

//async function submitVote(event) {
//    event.preventDefault();

//    if (!clientId) {
//        alert("Error: No client ID assigned. Please refresh page.");
//        return;
//    }

//    const selected = document.querySelector('input[name="candidate"]:checked').value;
//    await fetch('http://127.0.0.1:8080/vote', {
//        method: 'POST',
//        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
//        body: clientId + '&candidate=' + encodeURIComponent(selected)
//    });
//    voteSubmitted = true;
//    showScreen('submittedScreen');
//    startCountdown();
//    startFetchingStats();
//}
//function startCountdown() {
//    countdownInterval = setInterval(async () => {
//        const res = await fetch('http://127.0.0.1:8080/status');
//        const status = await res.text();
//        if (status.startsWith('voting')) {
//            const parts = status.split('|');
//            secondsLeft = parseInt(parts[1]);

//            let minutes = Math.floor(secondsLeft / 60);
//            let seconds = secondsLeft % 60;
//            document.getElementById('countdownTimer').textContent =
//                `Voting ends in ${String(minutes).padStart(2, '0')}:${String(seconds).padStart(2, '0')}`;
//        }
//        else if (status.startsWith('ended')) {
//            clearInterval(countdownInterval);
//            clearInterval(statsInterval);
//            getResults();
//        }
//    }, 1000);
//}

//async function getResults() {
//    const res = await fetch('http://127.0.0.1:8080/results');
//    const data = await res.json();

//    document.getElementById('winnerName').textContent = "Winner: " + data.winner;

//    const chartDiv = document.getElementById('resultsChart');
//    chartDiv.innerHTML = "";

//    let colorIndex = 0;
//    for (const [candidate, votes] of Object.entries(data.votes)) {
//        const bar = document.createElement('div');
//        bar.style.background = COLORS[colorIndex % COLORS.length];
//        bar.style.height = '30px';
//        bar.style.width = (votes * 40) + 'px';
//        bar.style.margin = '10px';
//        bar.style.color = 'white';
//        bar.style.padding = '5px';
//        bar.style.borderRadius = '8px';
//        bar.textContent = `${candidate} (${votes})`;
//        chartDiv.appendChild(bar);
//        colorIndex++;
//    }

//    showScreen('resultsScreen');
//}

//async function fetchStats() {
//    const res = await fetch('http://127.0.0.1:8080/stats');
//    const data = await res.json();

//    const chartDiv = document.getElementById('statsChart');
//    if (!chartDiv) return;

//    chartDiv.innerHTML = "";

//    let colorIndex = 0;
//    for (const [candidate, votes] of Object.entries(data.votes)) {
//        const bar = document.createElement('div');
//        bar.style.background = COLORS[colorIndex % COLORS.length];
//        bar.style.height = '20px';
//        bar.style.width = (votes * 30) + 'px';
//        bar.style.margin = '5px';
//        bar.style.color = 'white';
//        bar.style.padding = '5px';
//        bar.style.borderRadius = '8px';
//        bar.textContent = `${candidate}: ${votes}`;
//        chartDiv.appendChild(bar);
//        colorIndex++;
//    }
//}

//function startFetchingStats() {
//    statsInterval = setInterval(fetchStats, 3000);
//}

//function showScreen(id) {
//    document.querySelectorAll('.screen').forEach(div => div.classList.add('hidden'));
//    document.getElementById(id).classList.remove('hidden');
//}

//document.getElementById('voteForm').addEventListener('submit', submitVote);

//connectClient();
//statusInterval = setInterval(checkStatus, 3000);