s2e = {
  kleeArgs = {
    "--state-shared-memory=true",
    "--flush-tbs-on-state-switch=false",
    "--use-concolic-execution=true",
    "--use-dfs-search=true",
    "--enable-speculative-forking=false"
  }
}
plugins = {
  "BaseInstructions", "ExecutionTracer", "InterpreterMonitor", "ConcolicSession"
}

pluginsConfig = {}

pluginsConfig.ConcolicSession = {
    stopOnError = false,
    useRandomPending = true,
    useWeighting = false
}

