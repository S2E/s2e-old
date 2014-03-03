s2e = {
  kleeArgs = {
    "--state-shared-memory=true",
    "--flush-tbs-on-state-switch=false",
    "--use-concolic-execution=true",
    "--use-dfs-search=true",
    "--enable-speculative-forking=false"
--    "--use-query-pc-log=true"
  }
}
plugins = {
  "BaseInstructions", "ExecutionTracer", "InterpreterMonitor", "ConcolicSession"
}

pluginsConfig = {}

pluginsConfig.ConcolicSession = {
    stopOnError = false,
    useRandomPending = false,
    usePredictorService = false,
    stateTimeOut = 180,
    --seedsFile = "/home/stefan/nice-0.7.2-svn/sym_exec/nice_seeds_2.dat"
}
