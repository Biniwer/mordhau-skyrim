# Dev: press a button (by its text) in a window of a process, through UI Automation - for ordinary program windows
# where injected mouse clicks did not land (DynDOLOD / TexGen dialogs).
# usage: uiclick.ps1 -Proc DynDOLODx64 -Button High      |   -List prints the buttons and check boxes it can see
param([Parameter(Mandatory = $true)][string]$Proc, [string]$Button = '', [switch]$List)
Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes
$A = [System.Windows.Automation.AutomationElement]
$T = [System.Windows.Automation.TreeScope]
$ids = @(Get-Process $Proc -ErrorAction SilentlyContinue | ForEach-Object Id)
if (-not $ids) { "no process $Proc"; exit 1 }
$found = $false
foreach ($w in $A::RootElement.FindAll($T::Children, [System.Windows.Automation.Condition]::TrueCondition)) {
    if ($ids -notcontains $w.Current.ProcessId) { continue }
    foreach ($e in $w.FindAll($T::Descendants, [System.Windows.Automation.Condition]::TrueCondition)) {
        $type = $e.Current.ControlType.ProgrammaticName -replace 'ControlType\.', ''
        if ($type -notin 'Button', 'CheckBox') { continue }
        $name = $e.Current.Name
        if ($List) { "[$($w.Current.Name)] $type '$name'"; continue }
        if ($name -eq $Button -and -not $found) {
            $p = $null
            if ($e.TryGetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern, [ref]$p)) { $p.Invoke(); $found = $true; "pressed '$name' in [$($w.Current.Name)]" }
            elseif ($e.TryGetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern, [ref]$p)) { $p.Toggle(); $found = $true; "toggled '$name' in [$($w.Current.Name)]" }
        }
    }
}
if (-not $List -and -not $found) { "no button '$Button' found"; exit 1 }
