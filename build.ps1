Write-Host "=========================================" -ForegroundColor Cyan
Write-Host "   Compiling Distributed KV Database     " -ForegroundColor Cyan
Write-Host "=========================================" -ForegroundColor Cyan

# Ensure output directory exists or compile directly in current directory
Write-Host "Compiling Server..." -ForegroundColor Yellow
g++ -std=c++20 -O3 -Wall src/server.cpp src/database.cpp src/sql_parser.cpp src/network.cpp src/replication.cpp -lws2_32 -o server.exe

if ($LASTEXITCODE -ne 0) {
    Write-Host "Failed to compile server.exe" -ForegroundColor Red
    exit $LASTEXITCODE
}
Write-Host "Successfully compiled server.exe" -ForegroundColor Green

Write-Host "Compiling Client..." -ForegroundColor Yellow
g++ -std=c++20 -O3 -Wall src/client.cpp src/network.cpp -lws2_32 -o client.exe

if ($LASTEXITCODE -ne 0) {
    Write-Host "Failed to compile client.exe" -ForegroundColor Red
    exit $LASTEXITCODE
}
Write-Host "Successfully compiled client.exe" -ForegroundColor Green
Write-Host "All binaries compiled successfully!" -ForegroundColor Green
