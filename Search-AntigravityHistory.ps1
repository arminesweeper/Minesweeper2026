
[CmdletBinding()]
param(
     [string]$RootPath = (Join-Path $env:USERPROFILE ".gemini\antigravity-ide\brain"),

     [string[]]$IncludePattern = @(
          "minesweeper", "mine.?sweeper", "robot.?hardware",
          "hardware.?troubleshoot", "\bPCB\b", "\bencoder(s)?\b",
          "\bArduino\b", "\bMega 2560\b", "\bSTM32\b",
          "\bfirmware\b", "\bPID\b", "motor.?control", "\bCytron\b",
          "Raspberry.?Pi", "\bESP8266\b", "\bESP.?01(S)?\b",
          "\bWi.?Fi\b", "\bROS.?2\b", "limit.?switch",
          "\bgripper\b", "\blift\b", "\bmagnet\b", "\bsiren\b",
          "\bproximity\b", "\bwatchdog\b", "\bdeadlock\b",
          "\bI2C\b", "\bPWM\b", "\bPPR\b", "\bwheel\b",
          "\btelemetry\b", "FreeRTOS", "electromagnet"
     ),

     [string[]]$ExcludePattern = @(),

     [ValidateSet("Any", "User", "Assistant", "Planner", "Tool")]
     [string]$MessageType = "Any",

     [datetime]$Since,
     [datetime]$Until,

     [switch]$ShowMatches,
     [switch]$ShowFullMessages,
     [switch]$ShowContext,
     [switch]$ListAllConversations,

     [ValidateRange(0, 20)]
     [int]$ContextLines = 2,

     [ValidateRange(1, 1000)]
     [int]$MaxMatchesPerConversation = 50,

     [ValidateRange(1, 10000)]
     [int]$MaxConversations = 0,

     [ValidateRange(50, 10000)]
     [int]$SnippetLength = 350,

     [string]$ExportCsv,
     [string]$ExportJson,
     [string]$ExportMarkdown,
     [string]$ExportMessagesDirectory,

     [switch]$Quiet
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

# ------------------------------------------------------------
# Helpers
# ------------------------------------------------------------

function Get-PropertyValue {
     param(
          [object]$Object,
          [string[]]$Names
     )

     foreach ($name in $Names) {
          if ($null -ne $Object) {
               $property = $Object.PSObject.Properties[$name]

               if ($null -ne $property -and $null -ne $property.Value) {
                    return $property.Value
               }
          }
     }

     return $null
}

function Convert-ValueToText {
     param([object]$Value)

     if ($null -eq $Value) {
          return ""
     }

     if ($Value -is [string]) {
          return $Value
     }

     if ($Value -is [System.Array]) {
          $parts = foreach ($item in $Value) {
               Convert-ValueToText $item
          }

          return ($parts -join "`n")
     }

     if ($Value -is [pscustomobject]) {
          $parts = foreach ($property in $Value.PSObject.Properties) {
               if ($property.Value -is [string]) {
                    $property.Value
               }
               elseif ($null -ne $property.Value) {
                    ConvertTo-Json -InputObject $property.Value -Depth 12 -Compress
               }
          }

          return ($parts -join "`n")
     }

     return [string]$Value
}

function Get-NormalizedMessageType {
     param([string]$RawType)

     $type = ($RawType + "").ToUpperInvariant()

     if ($type -match "USER|HUMAN") {
          return "User"
     }

     if ($type -match "PLANNER") {
          return "Planner"
     }

     if ($type -match "ASSISTANT|MODEL|AI_RESPONSE") {
          return "Assistant"
     }

     if ($type -match "TOOL|COMMAND|EXEC|TERMINAL|BROWSER") {
          return "Tool"
     }

     return "Unknown"
}

function Test-AnyPattern {
     param(
          [string]$Text,
          [string[]]$Patterns
     )

     foreach ($pattern in $Patterns) {
          if (
               -not [string]::IsNullOrWhiteSpace($pattern) -and
               $Text -match "(?i)$pattern"
          ) {
               return $true
          }
     }

     return $false
}

function Get-Categories {
     param([string]$Text)

     $rules = [ordered]@{
          Minesweeper   = 'minesweeper|mine.?sweeper'
          Robotics      = 'robot|ROV|UGV|micromouse|line.?follower'
          Hardware      = 'hardware|wiring|voltage|current|battery'
          PCB           = '\bPCB\b|schematic|routing|footprint'
          Encoder       = '\bencoder(s)?\b|quadrature|PPR'
          Arduino       = '\bArduino\b|ATmega2560|Mega 2560'
          STM32         = '\bSTM32\b|Blue Pill|STM32F103'
          Firmware      = '\bfirmware\b|embedded|interrupt|ISR'
          PID           = '\bPID\b|anti.?windup'
          MotorControl  = 'motor.?control|Cytron|MDD10A|PWM|DIR'
          RaspberryPi   = 'Raspberry.?Pi|RPI|ROS.?2'
          ESP8266       = '\bESP8266\b|ESP.?01(S)?|Wi.?Fi'
          Sensors       = 'sensor|MPU6050|proximity|metal detector|IMU'
          LiftGripper   = '\blift\b|\bgripper\b|limit.?switch'
          Communication = 'serial|UART|USB|protocol|telemetry|packet'
          Safety        = 'watchdog|emergency.?stop|E.?stop|timeout|failsafe|deadlock'
          MagnetRelay   = 'electromagnet|\bmagnet\b|relay|duty.?cycle'
          Siren         = '\bsiren\b|buzzer|active.?low|active.?high'
          Calibration   = 'calibrat|threshold|ADC|1023'
          RTOS          = 'RTOS|FreeRTOS|scheduler|lagging'
     }

     $found = @()

     foreach ($name in $rules.Keys) {
          if ($Text -match "(?i)$($rules[$name])") {
               $found += $name
          }
     }

     return ($found -join "; ")
}

function Get-Snippet {
     param(
          [string]$Text,
          [int]$Length
     )

     $clean = [regex]::Replace(
          ($Text -replace "`r", ""),
          '\s+',
          ' '
     ).Trim()

     if ($clean.Length -le $Length) {
          return $clean
     }

     return $clean.Substring(0, $Length).TrimEnd() + "..."
}

function Export-ParentDirectory {
     param([string]$Path)

     $parent = Split-Path -Parent $Path

     if ($parent -and -not (Test-Path -LiteralPath $parent)) {
          New-Item -ItemType Directory -Path $parent -Force |
          Out-Null
     }
}

# ------------------------------------------------------------
# Validate root and discover transcripts
# ------------------------------------------------------------

if (-not (Test-Path -LiteralPath $RootPath -PathType Container)) {
     throw "Root folder not found: $RootPath"
}

$root = (Resolve-Path -LiteralPath $RootPath).Path

$transcriptFiles = @(
     Get-ChildItem -LiteralPath $root -Recurse -File `
          -Filter "transcript.jsonl" `
          -ErrorAction SilentlyContinue |
     Sort-Object FullName
)

if ($transcriptFiles.Count -eq 0) {
     throw "No transcript.jsonl files found under: $root"
}

$conversations = New-Object System.Collections.Generic.List[object]
$allMatches = New-Object System.Collections.Generic.List[object]
$scanned = 0

# ------------------------------------------------------------
# Scan each conversation
# ------------------------------------------------------------

foreach ($file in $transcriptFiles) {
     if ($MaxConversations -gt 0 -and $scanned -ge $MaxConversations) {
          break
     }

     # Expected:
     # brain\<conversation-id>\.system_generated\logs\transcript.jsonl

     $conversationDir = Split-Path `
     (Split-Path $file.FullName -Parent) -Parent

     $conversationId = Split-Path $conversationDir -Leaf

     $conversationItem = Get-Item `
          -LiteralPath $conversationDir `
          -ErrorAction SilentlyContinue

     if ($null -ne $conversationItem) {
          $conversationTime = $conversationItem.LastWriteTime
     }
     else {
          $conversationTime = $file.LastWriteTime
     }

     if (
          $PSBoundParameters.ContainsKey("Since") -and
          $conversationTime -lt $Since
     ) {
          continue
     }

     if (
          $PSBoundParameters.ContainsKey("Until") -and
          $conversationTime -gt $Until
     ) {
          continue
     }

     $scanned++

     $messages = New-Object System.Collections.Generic.List[object]
     $lineNumber = 0
     $firstUserMessage = ""
     $parseErrors = 0

     # Read line-by-line instead of loading the entire transcript into RAM.
     foreach ($line in [System.IO.File]::ReadLines($file.FullName)) {
          $lineNumber++

          if ([string]::IsNullOrWhiteSpace($line)) {
               continue
          }

          try {
               $obj = $line | ConvertFrom-Json -ErrorAction Stop
          }
          catch {
               $parseErrors++
               continue
          }

          $rawType = [string](Get-PropertyValue $obj @(
                    "type", "role", "messageType", "event"
               ))

          $normalizedType = Get-NormalizedMessageType $rawType

          $contentValue = Get-PropertyValue $obj @(
               "content", "text", "message", "prompt", "response"
          )

          $content = Convert-ValueToText $contentValue

          # Fallback for unfamiliar transcript schemas.
          if ([string]::IsNullOrWhiteSpace($content)) {
               $content = Convert-ValueToText $obj
          }

          if ([string]::IsNullOrWhiteSpace($content)) {
               continue
          }

          if (
               [string]::IsNullOrWhiteSpace($firstUserMessage) -and
               $normalizedType -eq "User"
          ) {
               $firstUserMessage = Get-Snippet $content 220
          }

          $timestampValue = Get-PropertyValue $obj @(
               "timestamp", "createdAt", "time", "date"
          )

          $messages.Add([pscustomobject]@{
                    ConversationId = $conversationId
                    LineNumber     = $lineNumber
                    Type           = $normalizedType
                    RawType        = $rawType
                    Content        = $content
                    Timestamp      = [string]$timestampValue
               })
     }

     $conversationMatches = New-Object System.Collections.Generic.List[object]
     $totalMatchCount = 0

     foreach ($message in $messages) {
          $typeAllowed = switch ($MessageType) {
               "Any" { $true }
               "User" { $message.Type -eq "User" }
               "Assistant" { $message.Type -eq "Assistant" }
               "Planner" { $message.Type -eq "Planner" }
               "Tool" { $message.Type -eq "Tool" }
          }

          $isMatch = $typeAllowed -and (
               Test-AnyPattern $message.Content $IncludePattern
          )

          if (
               $isMatch -and
               $ExcludePattern.Count -gt 0 -and
               (Test-AnyPattern $message.Content $ExcludePattern)
          ) {
               $isMatch = $false
          }

          if (-not $isMatch) {
               continue
          }

          $totalMatchCount++

          if ($conversationMatches.Count -ge $MaxMatchesPerConversation) {
               continue
          }

          $record = [pscustomobject]@{
               ConversationId            = $conversationId
               ConversationLastWriteTime = $conversationTime
               TranscriptPath            = $file.FullName
               LineNumber                = $message.LineNumber
               Type                      = $message.Type
               RawType                   = $message.RawType
               Categories                = Get-Categories $message.Content
               Timestamp                 = $message.Timestamp
               Snippet                   = Get-Snippet $message.Content $SnippetLength
               Content                   = $message.Content
          }

          $conversationMatches.Add($record)
          $allMatches.Add($record)
     }

     if ($ListAllConversations -or $totalMatchCount -gt 0) {
          $categoryList = @(
               $conversationMatches |
               ForEach-Object { $_.Categories -split '; ' } |
               Where-Object { $_ } |
               Sort-Object -Unique
          ) -join "; "

          $conversations.Add([pscustomobject]@{
                    ConversationId   = $conversationId
                    LastWriteTime    = $conversationTime
                    MessageCount     = $messages.Count
                    MatchCount       = $totalMatchCount
                    Categories       = $categoryList
                    FirstUserMessage = $firstUserMessage
                    TranscriptPath   = $file.FullName
                    ParseErrors      = $parseErrors
               })

          if (-not $Quiet) {
               Write-Host (
                    "MATCH: {0} | {1} | messages={2} | matches={3}" -f
                    $conversationId, $conversationTime,
                    $messages.Count, $totalMatchCount
               ) -ForegroundColor Cyan

               if ($firstUserMessage) {
                    Write-Host "  First user message: $firstUserMessage"
               }
          }
     }

     # --------------------------------------------------------
     # Optional detailed matches and surrounding context
     # --------------------------------------------------------

     if ($ShowMatches) {
          foreach ($hit in $conversationMatches) {
               Write-Host (
                    "[{0}:{1}] {2}" -f
                    $hit.Type, $hit.LineNumber, $hit.Categories
               ) -ForegroundColor Yellow

               if ($ShowFullMessages) {
                    Write-Host $hit.Content
               }
               else {
                    Write-Host ("  " + $hit.Snippet)
               }

               if ($ShowContext -and $ContextLines -gt 0) {
                    $index = -1

                    for ($i = 0; $i -lt $messages.Count; $i++) {
                         if ($messages[$i].LineNumber -eq $hit.LineNumber) {
                              $index = $i
                              break
                         }
                    }

                    if ($index -ge 0) {
                         $start = [Math]::Max(0, $index - $ContextLines)
                         $end = [Math]::Min(
                              $messages.Count - 1,
                              $index + $ContextLines
                         )

                         for ($i = $start; $i -le $end; $i++) {
                              if ($i -eq $index) {
                                   continue
                              }

                              Write-Host (
                                   "  Context [{0}:{1}] {2}" -f
                                   $messages[$i].Type,
                                   $messages[$i].LineNumber,
                                   (Get-Snippet $messages[$i].Content $SnippetLength)
                              ) -ForegroundColor DarkGray
                         }
                    }
               }

               Write-Host ""
          }
     }
}

# ------------------------------------------------------------
# Results and summary
# ------------------------------------------------------------

$conversationOutput = @(
     $conversations | Sort-Object LastWriteTime -Descending
)

$matchOutput = @(
     $allMatches |
     Sort-Object `
     @{ Expression = "ConversationLastWriteTime"; Descending = $true },
     @{ Expression = "ConversationId"; Descending = $false },
     @{ Expression = "LineNumber"; Descending = $false }
)

if (-not $Quiet) {
     Write-Host "========== SEARCH SUMMARY ==========" -ForegroundColor Green
     Write-Host "Root: $root"
     Write-Host "Transcripts discovered: $($transcriptFiles.Count)"
     Write-Host "Conversations scanned: $scanned"
     Write-Host "Conversations with matches: $($conversationOutput.Count)"
     Write-Host "Matching messages collected: $($matchOutput.Count)"
     Write-Host "Message type: $MessageType"

     $conversationOutput |
     Select-Object ConversationId, LastWriteTime, MessageCount,
     MatchCount, Categories, FirstUserMessage |
     Format-Table -AutoSize -Wrap
}

# ------------------------------------------------------------
# CSV export
# ------------------------------------------------------------

if ($ExportCsv) {
     Export-ParentDirectory $ExportCsv

     $matchOutput |
     Select-Object ConversationId, ConversationLastWriteTime,
     LineNumber, Type, Categories, Timestamp, Snippet, TranscriptPath |
     Export-Csv -LiteralPath $ExportCsv -NoTypeInformation -Encoding UTF8

     Write-Host "CSV exported: $ExportCsv"
}

# ------------------------------------------------------------
# JSON export
# ------------------------------------------------------------

if ($ExportJson) {
     Export-ParentDirectory $ExportJson

     [pscustomobject]@{
          GeneratedAt   = (Get-Date).ToString("o")
          RootPath      = $root
          ScannedCount  = $scanned
          Conversations = $conversationOutput
          Matches       = $matchOutput
     } |
     ConvertTo-Json -Depth 10 |
     Set-Content -LiteralPath $ExportJson -Encoding UTF8

     Write-Host "JSON exported: $ExportJson"
}

# ------------------------------------------------------------
# Markdown report export
# ------------------------------------------------------------

if ($ExportMarkdown) {
     Export-ParentDirectory $ExportMarkdown

     $md = New-Object System.Collections.Generic.List[string]

     $md.Add("# Antigravity Transcript Search Report")
     $md.Add("")
     $md.Add("- Generated: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')")
     $md.Add("- Root: ``$root``")
     $md.Add("- Conversations scanned: $scanned")
     $md.Add("- Conversations with matches: $($conversationOutput.Count)")
     $md.Add("")

     foreach ($conversation in $conversationOutput) {
          $md.Add("## Conversation $($conversation.ConversationId)")
          $md.Add("")
          $md.Add("- Last write: $($conversation.LastWriteTime)")
          $md.Add("- Messages: $($conversation.MessageCount)")
          $md.Add("- Matches: $($conversation.MatchCount)")
          $md.Add("- Categories: $($conversation.Categories)")
          $md.Add("- First user message: $($conversation.FirstUserMessage)")
          $md.Add("- Transcript: ``$($conversation.TranscriptPath)``")
          $md.Add("")

          $conversationHits = @(
               $matchOutput | Where-Object {
                    $_.ConversationId -eq $conversation.ConversationId
               }
          )

          foreach ($hit in $conversationHits) {
               $md.Add("### Line $($hit.LineNumber) — $($hit.Type)")
               $md.Add("")
               $md.Add("Categories: $($hit.Categories)")
               $md.Add("")
               $md.Add('```text')
               $md.Add(($hit.Content -replace '```', '[triple-backtick]'))
               $md.Add('```')
               $md.Add("")
          }
     }

     $md | Set-Content -LiteralPath $ExportMarkdown -Encoding UTF8
     Write-Host "Markdown report exported: $ExportMarkdown"
}

# ------------------------------------------------------------
# Separate Markdown file per matching conversation
# ------------------------------------------------------------

if ($ExportMessagesDirectory) {
     if (-not (Test-Path -LiteralPath $ExportMessagesDirectory)) {
          New-Item -ItemType Directory -Path $ExportMessagesDirectory -Force |
          Out-Null
     }

     foreach ($conversation in $conversationOutput) {
          $safeName = $conversation.ConversationId -replace '[^\w.-]', '_'
          $outputPath = Join-Path $ExportMessagesDirectory "$safeName.md"
          $md = New-Object System.Collections.Generic.List[string]

          $md.Add("# Conversation $($conversation.ConversationId)")
          $md.Add("")
          $md.Add("Last write: $($conversation.LastWriteTime)")
          $md.Add("")

          $conversationHits = @(
               $matchOutput | Where-Object {
                    $_.ConversationId -eq $conversation.ConversationId
               }
          )

          foreach ($hit in $conversationHits) {
               $md.Add("## Line $($hit.LineNumber) — $($hit.Type)")
               $md.Add("")
               $md.Add("Categories: $($hit.Categories)")
               $md.Add("")
               $md.Add('```text')
               $md.Add(($hit.Content -replace '```', '[triple-backtick]'))
               $md.Add('```')
               $md.Add("")
          }

          $md | Set-Content -LiteralPath $outputPath -Encoding UTF8
     }

     Write-Host "Per-conversation Markdown files: $ExportMessagesDirectory"
}

# Return objects for optional use by the PowerShell pipeline.
[pscustomobject]@{
     RootPath                 = $root
     TranscriptsDiscovered    = $transcriptFiles.Count
     ConversationsScanned     = $scanned
     ConversationsWithMatches = $conversationOutput
     Matches                  = $matchOutput
}
