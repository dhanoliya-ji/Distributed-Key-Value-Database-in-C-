# Automated Test Suite for Distributed Key-Value Database
$ErrorActionPreference = "Stop"

Write-Host "=========================================" -ForegroundColor Cyan
Write-Host " Running Distributed Database Test Suite  " -ForegroundColor Cyan
Write-Host "=========================================" -ForegroundColor Cyan

# 1. Compile binaries
Write-Host "[Test] Triggering build..." -ForegroundColor Yellow
.\build.ps1
if ($LASTEXITCODE -ne 0) {
    Write-Host "[Test] Build failed. Aborting." -ForegroundColor Red
    exit 1
}

# 2. Cleanup old db logs and snapshots to start fresh
Write-Host "[Test] Cleaning up old logs and snapshots..." -ForegroundColor Yellow
Remove-Item -Path "node_leader_wal.log", "node_leader_snapshot.db", "node_follower_wal.log", "node_follower_snapshot.db" -ErrorAction SilentlyContinue

# 3. Start Leader Server (Port 8000)
Write-Host "[Test] Starting Leader on port 8000..." -ForegroundColor Yellow
$leaderProc = Start-Process -FilePath ".\server.exe" -ArgumentList "--port 8000 --role leader --node-id node_leader" -NoNewWindow -PassThru
Start-Sleep -Seconds 2

# 4. Start Follower Server (Port 8001)
Write-Host "[Test] Starting Follower on port 8001..." -ForegroundColor Yellow
$followerProc = Start-Process -FilePath ".\server.exe" -ArgumentList "--port 8001 --role follower --leader-ip 127.0.0.1 --leader-port 8000 --node-id node_follower" -NoNewWindow -PassThru
Start-Sleep -Seconds 2

# Helper to send command to node and return response
function Send-Command($port, $command) {
    $client = New-Object System.Net.Sockets.TcpClient("127.0.0.1", $port)
    $stream = $client.GetStream()
    $writer = New-Object System.IO.StreamWriter($stream)
    $reader = New-Object System.IO.StreamReader($stream)
    
    $writer.WriteLine($command)
    $writer.Flush()
    
    $response = ""
    while ($true) {
        $line = $reader.ReadLine()
        if ($line -eq $null -or $line -eq [char]4) {
            break
        }
        $response += $line + "`n"
    }
    
    $reader.Close()
    $writer.Close()
    $client.Close()
    return $response.Trim()
}

$testsPassed = 0
$totalTests = 0

function Assert-Contains($value, $substring, $testName) {
    $global:totalTests++
    if ($value -like "*$substring*") {
        Write-Host "  [PASS] $testName" -ForegroundColor Green
        $global:testsPassed++
    } else {
        Write-Host "  [FAIL] $testName" -ForegroundColor Red
        Write-Host "    Expected to contain: '$substring'" -ForegroundColor DarkRed
        Write-Host "    Got:                 '$value'" -ForegroundColor DarkRed
    }
}

try {
    # Test 1: Write raw PUT on Leader
    Write-Host "[Test 1] Testing raw PUT on Leader..." -ForegroundColor Yellow
    $res = Send-Command 8000 "PUT user:100 Administrator"
    Assert-Contains $res "OK" "Leader accepted raw PUT"

    # Test 2: Read raw GET on Leader
    Write-Host "[Test 2] Testing raw GET on Leader..." -ForegroundColor Yellow
    $res = Send-Command 8000 "GET user:100"
    Assert-Contains $res "VALUE Administrator" "Leader returned raw GET value"

    # Test 3: SQL INSERT on Leader
    Write-Host "[Test 3] Testing SQL INSERT on Leader..." -ForegroundColor Yellow
    $res = Send-Command 8000 "INSERT INTO kv (key, value) VALUES ('user:200', 'JaneDoe');"
    Assert-Contains $res "Query OK, 1 row affected" "Leader SQL INSERT succeeded"

    # Test 4: SQL SELECT on Leader
    Write-Host "[Test 4] Testing SQL SELECT on Leader..." -ForegroundColor Yellow
    $res = Send-Command 8000 "SELECT value FROM kv WHERE key = 'user:200';"
    Assert-Contains $res "JaneDoe" "Leader SQL SELECT returned correct value"

    # Test 5: Verify Replication on Follower
    Write-Host "[Test 5] Verifying replication on Follower..." -ForegroundColor Yellow
    Start-Sleep -Seconds 1 # Wait for replication propagation
    $res = Send-Command 8001 "SELECT value FROM kv WHERE key = 'user:200';"
    Assert-Contains $res "JaneDoe" "Follower replicated user:200 from Leader"

    $res = Send-Command 8001 "GET user:100"
    Assert-Contains $res "VALUE Administrator" "Follower replicated user:100 from Leader"

    # Test 6: Verify Follower rejects Writes
    Write-Host "[Test 6] Verifying write rejection on Follower..." -ForegroundColor Yellow
    $res = Send-Command 8001 "INSERT INTO kv VALUES ('user:300', 'Hacker');"
    Assert-Contains $res "ERROR: Node is a Follower. Write operations are read-only." "Follower blocked SQL write"

    $res = Send-Command 8001 "PUT user:300 Hacker"
    Assert-Contains $res "ERROR: Node is a Follower. Write operations are read-only." "Follower blocked raw write"

    # Test 7: SQL UPDATE on Leader and Replication check
    Write-Host "[Test 7] Testing SQL UPDATE and replication..." -ForegroundColor Yellow
    $res = Send-Command 8000 "UPDATE kv SET value = 'JaneSmith' WHERE key = 'user:200';"
    Assert-Contains $res "Query OK, 1 row affected" "Leader SQL UPDATE succeeded"

    Start-Sleep -Seconds 1
    $res = Send-Command 8001 "SELECT value FROM kv WHERE key = 'user:200';"
    Assert-Contains $res "JaneSmith" "Follower replicated updated value"

    # Test 8: Select All from Follower
    Write-Host "[Test 8] Testing SELECT ALL on Follower..." -ForegroundColor Yellow
    $res = Send-Command 8001 "SELECT * FROM kv;"
    Assert-Contains $res "user:100" "SELECT ALL contains user:100"
    Assert-Contains $res "user:200" "SELECT ALL contains user:200"

    # Test 9: Snapshot Compaction on Leader
    Write-Host "[Test 9] Creating snapshot on Leader..." -ForegroundColor Yellow
    $res = Send-Command 8000 "SNAPSHOT"
    Assert-Contains $res "OK" "Leader completed snapshot compaction"

    # Check that snapshot file was created
    if (Test-Path "node_leader_snapshot.db") {
        Write-Host "  [PASS] node_leader_snapshot.db exists" -ForegroundColor Green
        $testsPassed++
    } else {
        Write-Host "  [FAIL] node_leader_snapshot.db does not exist" -ForegroundColor Red
    }
    $totalTests++

    # Test 10: Server Restart & Data Recovery
    Write-Host "[Test 10] Testing Server Restart & Recovery..." -ForegroundColor Yellow
    
    # Kill servers
    Write-Host "  Stopping servers..." -ForegroundColor DarkYellow
    Stop-Process -Id $leaderProc.Id -Force
    Stop-Process -Id $followerProc.Id -Force
    Start-Sleep -Seconds 1

    # Restart Leader Server
    Write-Host "  Restarting Leader..." -ForegroundColor DarkYellow
    $leaderProc = Start-Process -FilePath ".\server.exe" -ArgumentList "--port 8000 --role leader --node-id node_leader" -NoNewWindow -PassThru
    Start-Sleep -Seconds 2

    # Query restarted Leader to verify recovery
    $res = Send-Command 8000 "SELECT value FROM kv WHERE key = 'user:200';"
    Assert-Contains $res "JaneSmith" "Leader successfully recovered database state on startup"

} finally {
    # Cleanup processes
    Write-Host "[Test] Cleaning up running server processes..." -ForegroundColor Yellow
    if ($leaderProc -ne $null) {
        Stop-Process -Id $leaderProc.Id -Force -ErrorAction SilentlyContinue
    }
    if ($followerProc -ne $null) {
        Stop-Process -Id $followerProc.Id -Force -ErrorAction SilentlyContinue
    }
}

Write-Host "=========================================" -ForegroundColor Cyan
Write-Host " Test Summary: $testsPassed / $totalTests Passed" -ForegroundColor Cyan
Write-Host "=========================================" -ForegroundColor Cyan

if ($testsPassed -eq $totalTests) {
    exit 0
} else {
    exit 1
}
