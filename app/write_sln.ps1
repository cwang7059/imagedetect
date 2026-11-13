$guidMatch = Select-String -Path (Join-Path $PSScriptRoot "imagedetect.vcxproj") -Pattern 'ProjectGuid>(\{[^}]+\})'
$guid = if ($guidMatch) { $guidMatch.Matches[0].Groups[1].Value } else { '{92E7F5D2-8AC5-3D89-89BC-1D2CD4256008}' }

$sln = @"
Microsoft Visual Studio Solution File, Format Version 12.00
# Visual Studio Version 18
VisualStudioVersion = 18.0.11222.15
MinimumVisualStudioVersion = 10.0.40219.1
Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "imagedetect", "imagedetect.vcxproj", "$guid"
EndProject
Global
	GlobalSection(SolutionConfigurationPlatforms) = preSolution
		Debug|x64 = Debug|x64
		Release|x64 = Release|x64
	EndGlobalSection
	GlobalSection(ProjectConfigurationPlatforms) = postSolution
		$guid.Debug|x64.ActiveCfg = Debug|x64
		$guid.Debug|x64.Build.0 = Debug|x64
		$guid.Release|x64.ActiveCfg = Release|x64
		$guid.Release|x64.Build.0 = Release|x64
	EndGlobalSection
	GlobalSection(SolutionProperties) = preSolution
		HideSolutionNode = FALSE
	EndGlobalSection
EndGlobal
"@

Set-Content -Path (Join-Path $PSScriptRoot "imagedetect.sln") -Value $sln -Encoding ASCII
Write-Host "Wrote imagedetect.sln guid=$guid"
