param(
    [int]$elo = 2400,
    [string]$tc = "1+0.1",
    [int]$games = 100
)

$cutechess = "C:\Program Files (x86)\Cute Chess\cutechess-cli.exe"
$stockfish = "C:\Stockfish\Stockfish\src\stockfish.exe"
$engine = "build\Release\chess-engine.exe"

if (!(Test-Path $cutechess)) {
    Write-Host "ERROR: cutechess not found at $cutechess" -ForegroundColor Red
    exit 1
}

if (!(Test-Path $stockfish)) {
    Write-Host "ERROR: stockfish not found at $stockfish" -ForegroundColor Red
    exit 1
}

if (!(Test-Path $engine)) {
    Write-Host "ERROR: engine not found at $engine" -ForegroundColor Red
    exit 1
}

Write-Host "Using cutechess: $cutechess" -ForegroundColor Green
Write-Host "Using stockfish: $stockfish" -ForegroundColor Green
Write-Host "Using engine: $engine" -ForegroundColor Green
Write-Host ""
Write-Host "Testing: $engine vs Stockfish $elo" -ForegroundColor Cyan
Write-Host "Time control: $tc" -ForegroundColor Cyan
Write-Host "Games: $games (rounds = $($games / 2))" -ForegroundColor Cyan
Write-Host ""

$rounds = [math]::Ceiling($games / 2)
$tc_clean = $tc -replace '\+', '_'
$output = "test_results_sf$elo`_$tc_clean.pgn"

Write-Host "Output: $output" -ForegroundColor Cyan
Write-Host "Starting test..." -ForegroundColor Yellow
Write-Host ""

& "$cutechess" `
    -engine "cmd=$engine" proto=uci `
    -engine "cmd=$stockfish" proto=uci `
    -each "tc=$tc" `
    -rounds $rounds `
    -games 2 `
    -repeat `
    -resign movecount=3 score=400 `
    -draw movenumber=40 movecount=6 score=10 `
    -pgnout "$output" `
    -concurrency 1
