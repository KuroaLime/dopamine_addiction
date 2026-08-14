# CsvProfiler 결과 분석 헬퍼.
#   . .\Analyze-Csv.ps1
#   Show-Perf -Files @{ 'baseline'='Profile(...).csv'; 'test'='Profile(...).csv' }
#   Get-LatestProfiles -Count 2

$script:CsvDir = 'C:\Users\3dlst\Documents\GitHub\Project-for-Graduation\Project\Manager\Saved\Profiling\CSV'

$script:DefaultCounters = @(
    'GPUTime'
    'GameThreadTime'
    'RHI/DrawCalls'
    'RHI/PrimitivesDrawn'
    'Exclusive/GameThread/UI'
    'Exclusive/GameThread/NavigationBuild'
    'Exclusive/GameThread/TickActors'
    'Exclusive/GameThread/DeferredTickTime'
    'Exclusive/RenderThread/RenderShadows'
    'Exclusive/RenderThread/RenderBasePass'
    'Exclusive/RenderThread/RenderPostProcessing'
    'Exclusive/AllWorkers/Physics'
)

function Get-LatestProfiles {
    param([int]$Count = 2)
    Get-ChildItem $script:CsvDir -Filter *.csv |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First $Count Name, LastWriteTime
}

# CSV에서 지정 컬럼만 1회 파싱해 뽑는다. 중복 컬럼명은 첫 번째만 사용.
function Read-Counters {
    param([string]$Path, [string[]]$Counters)
    $lines = Get-Content $Path
    $hdr = $lines[0] -split ','
    $map = @{}
    foreach ($c in $Counters) {
        for ($i = 0; $i -lt $hdr.Count; $i++) { if ($hdr[$i] -eq $c) { $map[$c] = $i; break } }
    }
    $acc = @{}
    foreach ($k in $map.Keys) { $acc[$k] = New-Object System.Collections.ArrayList }
    for ($r = 1; $r -lt $lines.Count; $r++) {
        $f = $lines[$r] -split ','
        foreach ($k in $map.Keys) {
            $i = $map[$k]
            if ($i -lt $f.Count) { $x = 0.0; if ([double]::TryParse($f[$i], [ref]$x)) { [void]$acc[$k].Add($x) } }
        }
    }
    return $acc
}

function Get-Percentile {
    param($Values, [double]$P = 0.5)
    $v = @($Values | Where-Object { $_ -gt 0 })
    if (-not $v.Count) { return 0 }
    $s = $v | Sort-Object
    return [math]::Round($s[[int]($s.Count * $P)], 2)
}

# $Files: 순서 있는 해시테이블 @{ 라벨 = 파일명 }
function Show-Perf {
    param([hashtable]$Files, [string[]]$Counters = $script:DefaultCounters)
    $labels = @($Files.Keys)
    $data = @{}
    foreach ($l in $labels) {
        $p = Join-Path $script:CsvDir $Files[$l]
        $data[$l] = Read-Counters -Path $p -Counters $Counters
    }
    $rows = foreach ($c in $Counters) {
        $row = [ordered]@{ 카운터 = $c -replace 'Exclusive/', '' }
        foreach ($l in $labels) { $row[$l] = Get-Percentile $data[$l][$c] 0.5 }
        [PSCustomObject]$row
    }
    $rows | Format-Table -AutoSize
    foreach ($l in $labels) {
        $n = $data[$l]['GPUTime'].Count
        "{0,-16} 프레임={1}" -f $l, $n
    }
}
