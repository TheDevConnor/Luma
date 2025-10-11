// import * as path from "path";
import { workspace, ExtensionContext, commands, window } from "vscode";

import {
  LanguageClient,
  LanguageClientOptions,
  ServerOptions,
  TransportKind,
} from "vscode-languageclient/node";

let client: LanguageClient;

export function activate(context: ExtensionContext) {
  // If the extension is launched in debug mode then the debug server options are used
    // Otherwise the run options are used
  const serverOptions: ServerOptions = {
    command: "/home/TheDevConnor/Luma/luma",
    transport: TransportKind.stdio,
    args: ["-lsp"]
  };

  // Options to control the language client
  const clientOptions: LanguageClientOptions = {
    // Register the server for all documents by default
    documentSelector: [{ scheme: "file", language: "luma" }],
    synchronize: {
      // Notify the server about file changes to '.clientrc files contained in the workspace
      fileEvents: workspace.createFileSystemWatcher("**/.clientrc"),
    },
  };

  // Create the language client and start the client.
  client = new LanguageClient(
    "luma-lsp",
    "Luma LSP",
    serverOptions,
    clientOptions
  );
  // Add a command (accessible via ctrl+shift+p) for restarting the server
  const dispoable = commands.registerCommand(
    "luma.restartServer",
    async () => {
      if (!client) return;
      window.showInformationMessage("Restarting Luma LSP...");
      await client.stop();
      client = new LanguageClient(
        "luma-lsp",
        "Luma LSP",
        serverOptions,
        clientOptions
      );
      
      await client.start();
      window.showInformationMessage("Luma LSP restarted.");
    }
  )
  context.subscriptions.push(dispoable);

  // Start the client. This will also launch the server
  client.start();
}

export function deactivate(): Thenable<void> | undefined {
  if (!client) {
    return undefined;
  }
  return client.stop();
}
