# 把本地仓库推到 GitHub。用法：
#   powershell -ExecutionPolicy Bypass -File tools\push.ps1 -User 你的GitHub用户名
param(
    [Parameter(Mandatory = $true)][string]$User,
    [string]$Repo = "badherts-plugin"
)

Set-Location (Split-Path $PSScriptRoot -Parent)

if (-not $User) { Write-Error "需要 GitHub 用户名"; exit 1 }

$url = "https://github.com/$User/$Repo.git"

git remote remove origin 2>$null
git remote add origin $url

# GitHub 默认分支是 main
git branch -M main 2>$null

Write-Host "推送 $url ..." -ForegroundColor Cyan
git push -u origin main

if ($LASTEXITCODE -eq 0) {
    Write-Host "完成：https://github.com/$User/$Repo" -ForegroundColor Green
} else {
    Write-Host "推送失败。如果是 non-fast-forward，说明建仓库时勾了 README，先跑：" -ForegroundColor Yellow
    Write-Host "  git pull --rebase origin main" -ForegroundColor Yellow
}
