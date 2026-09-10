[CmdletBinding()]
param(
    [string]$GameExe = 'G:\SteamLibrary\steamapps\common\SOMA\Soma_NoSteam.exe',
    [string]$OutputJson
)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$source = Get-Content -LiteralPath (Join-Path $root 'src\dll\HPLHandsBootstrap.cpp') -Raw
$bytes = [IO.File]::ReadAllBytes($GameExe)
$hash = (Get-FileHash -LiteralPath $GameExe -Algorithm SHA256).Hash
if ($hash -ne '395CD54830C8E66E22166E71AC6FE95FD3BB5433B898737836744A6D178A0A6A') {
    throw 'This offline receipt only covers the registered NoSteam executable hash.'
}
$nt = [BitConverter]::ToInt32($bytes, 0x3c)
if ([BitConverter]::ToUInt32($bytes, $nt) -ne 0x4550 -or
    [BitConverter]::ToUInt16($bytes, $nt + 4) -ne 0x8664) { throw 'Not x64 PE' }
$count = [BitConverter]::ToUInt16($bytes, $nt + 6)
$sectionStart = $nt + 24 + [BitConverter]::ToUInt16($bytes, $nt + 20)
$sections = for ($i = 0; $i -lt $count; ++$i) {
    $s = $sectionStart + 40 * $i
    [pscustomobject]@{
        rva = [BitConverter]::ToUInt32($bytes, $s + 12)
        rawSize = [BitConverter]::ToUInt32($bytes, $s + 16)
        raw = [BitConverter]::ToUInt32($bytes, $s + 20)
        executable = ([BitConverter]::ToUInt32($bytes, $s + 36) -band 0x20000000) -ne 0
    }
}
function Get-Offset([uint32]$Rva, [int]$Size) {
    foreach ($s in $sections) {
        if ($Rva -ge $s.rva -and $Rva + $Size -le $s.rva + $s.rawSize) {
            return [int]($s.raw + $Rva - $s.rva)
        }
    }
    throw ('Unmapped RVA 0x{0:x}' -f $Rva)
}
# Latin1 is a bijective byte view; ordinal IndexOf gives bounded, overlapping matches.
$encoding = [Text.Encoding]::GetEncoding(28591)
$rows = foreach ($entry in @(
    @{name='kPostUpdate'; rva=0x1ab3a0}, @{name='kHasMethod'; rva=0x1dc170},
    @{name='kPrepare'; rva=0x1dd830}, @{name='kSetBool'; rva=0x1dc190},
    @{name='kExecute'; rva=0x1dd340}, @{name='kGetCurrentMap'; rva=0xcccb0}
)) {
    $match = [regex]::Match($source, "constexpr uint8_t $($entry.name)\[\] = \{([^}]+)\};")
    if (!$match.Success) { throw "Missing production signature $($entry.name)" }
    [byte[]]$signature = $match.Groups[1].Value.Split(',') | ForEach-Object {
        $token = $_.Trim()
        if ($token.StartsWith('0x')) { [Convert]::ToByte($token.Substring(2),16) }
        else { [byte]::Parse($token) }
    }
    $needle = $encoding.GetString($signature)
    $hits = 0
    foreach ($s in $sections | Where-Object executable) {
        $haystack = $encoding.GetString($bytes, $s.raw, $s.rawSize)
        $start = 0
        while ($start -le $haystack.Length - $needle.Length) {
            $pos = $haystack.IndexOf($needle, $start, [StringComparison]::Ordinal)
            if ($pos -lt 0) { break }
            ++$hits
            $start = $pos + 1
        }
    }
    $offset = Get-Offset $entry.rva $signature.Length
    $exact = $encoding.GetString($bytes, $offset, $signature.Length) -ceq $needle
    if (!$exact -or $hits -ne 1) { throw "Contract mismatch: $($entry.name) exact=$exact hits=$hits" }
    [pscustomobject]@{name=$entry.name; rva=('0x{0:x}' -f $entry.rva); bytes=$signature.Length; exact=$exact; matches=$hits}
}
$tables = foreach ($t in @(
    @{table=0x68fbb8;slot=0x30;callee=0x1ab3a0},
    @{table=0x6f2f48;slot=0x38;callee=0x54b3a0},
    @{table=0x6c0e38;slot=0x10;callee=0x484c50},
    @{table=0x6c0e38;slot=0x18;callee=0x484510},
    @{table=0x6c0e38;slot=0x70;callee=0x484860}
)) {
    $value = [BitConverter]::ToUInt64($bytes, (Get-Offset ($t.table + $t.slot) 8))
    if ($value -ne 0x140000000L + $t.callee) { throw 'Concrete virtual callee mismatch' }
    [pscustomobject]@{table=('0x{0:x}' -f $t.table);slot=('0x{0:x}' -f $t.slot);callee=('0x{0:x}' -f $value)}
}
$receipt = [ordered]@{
    environment='offline-pe-no-execution'; gameExe=$GameExe; sha256=$hash
    productionSourceSha256=(Get-FileHash -LiteralPath (Join-Path $root 'src\dll\HPLHandsBootstrap.cpp')).Hash
    signatures=@($rows); concreteVirtualCallees=@($tables)
    runtimeAcceptance='not-tested'; headsetAcceptance='not-tested'
}
$json = $receipt | ConvertTo-Json -Depth 8
if ($OutputJson) { $json | Set-Content -LiteralPath $OutputJson -Encoding utf8 }
$json
