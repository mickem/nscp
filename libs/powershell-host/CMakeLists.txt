cmake_minimum_required(VERSION 3.10)

# NSCP.PowerShell: the managed half of the PowerShellScript module, an ordinary
# NSClient++ .NET plugin that hosts the PowerShell engine. Built with the dotnet
# SDK (see NSCP_ADD_DOTNET_PROJECT) against the NSCP.Core.dll produced by
# libs/dotnet-plugin-api, into the same modules/dotnet folder.
#
# The PowerShell engine is not built or shipped here: the assembly is compiled
# against the System.Management.Automation reference assembly and loads the real
# one out of an installed PowerShell 7 at run time.
if(DOTNET_FOUND)
    project(NSCP.PowerShell)

    NSCP_ADD_DOTNET_PROJECT(
        NSCP.PowerShell
        ${CMAKE_CURRENT_SOURCE_DIR}/NSCP.PowerShell.csproj
        DEPENDS NSCP.Core
        PRODUCTS
            ${NSCP_DOTNET_OUTPUT_DIR}/NSCP.PowerShell.dll
            ${NSCP_DOTNET_OUTPUT_DIR}/NSCP.PowerShell.deps.json
    )

    install(
        FILES
            ${NSCP_DOTNET_OUTPUT_DIR}/NSCP.PowerShell.dll
            ${NSCP_DOTNET_OUTPUT_DIR}/NSCP.PowerShell.deps.json
        DESTINATION ${NSCP_DOTNET_INSTALL_DIR}
    )
endif()
