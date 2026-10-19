param([Parameter(Mandatory=$true)][string]$File)
$ErrorActionPreference = 'Stop'
$signature = Get-AuthenticodeSignature -LiteralPath $File
if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch '(^|,\s*)CN=Microsoft Corporation(,|$)') {
    throw 'Visual C++ Redistributable must have a valid Microsoft signature'
}
$info = (Get-Item -LiteralPath $File).VersionInfo
[Console]::WriteLine("$($info.FileMajorPart).$($info.FileMinorPart).$($info.FileBuildPart).$($info.FilePrivatePart)")
