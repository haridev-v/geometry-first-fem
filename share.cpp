using System;
using NXOpen;
using NXOpen.CAE;

namespace SimDataExtractor
{
    public static class NxSolutionRefreshScope
    {
        public static int RunWithClonedTemporarySolution(
            SimPart simPart,
            Session session,
            LogFile logFile,
            Func<int> action)
        {
            if (simPart == null)
                throw new ArgumentNullException(nameof(simPart));

            if (session == null)
                throw new ArgumentNullException(nameof(session));

            if (action == null)
                throw new ArgumentNullException(nameof(action));

            SimSimulation sim = simPart.Simulation;

            if (sim == null)
                throw new InvalidOperationException("SimPart.Simulation is null.");

            SimSolution originalSolution = null;
            SimSolution tempSolution = null;

            Session.UndoMarkId markId = Session.UndoMarkId.Null;

            try
            {
                originalSolution = sim.ActiveSolution;

                if (originalSolution == null)
                {
                    // If there is no active solution, do not invent one yet.
                    // Just run normally.
                    SafeLog(logFile, "No active solution found. Running FPD without cloned solution refresh.");
                    return action();
                }

                string tempName =
                    "__VECTA_TEMP_REFRESH_" +
                    DateTime.Now.ToString("yyyyMMdd_HHmmss");

                SafeLog(logFile, "Cloning active solution for temporary refresh: " + tempName);

                tempSolution = sim.CloneSolution(originalSolution, tempName);

                if (tempSolution == null)
                    throw new InvalidOperationException("CloneSolution returned null.");

                sim.ActiveSolution = tempSolution;

                markId = session.SetUndoMark(
                    Session.MarkVisibility.Invisible,
                    "VECTA temporary solution refresh");

                try
                {
                    sim.UpdateFemodel();
                    SafeLog(logFile, "sim.UpdateFemodel completed on temporary solution.");
                }
                catch (Exception ex)
                {
                    SafeLog(logFile, "sim.UpdateFemodel failed: " + ex.Message);
                }

                try
                {
                    int updateErrors = session.UpdateManager.DoUpdate(markId);
                    SafeLog(logFile, "UpdateManager.DoUpdate completed. errors=" + updateErrors);
                }
                catch (Exception ex)
                {
                    SafeLog(logFile, "UpdateManager.DoUpdate failed: " + ex.Message);
                }

                SafeLog(logFile, "Running action with temporary active solution.");
                return action();
            }
            finally
            {
                try
                {
                    if (originalSolution != null)
                    {
                        sim.ActiveSolution = originalSolution;
                        SafeLog(logFile, "Restored original active solution.");
                    }
                }
                catch (Exception ex)
                {
                    SafeLog(logFile, "Failed to restore original active solution: " + ex.Message);
                }

                try
                {
                    if (tempSolution != null)
                    {
                        sim.DeleteSolution(tempSolution);
                        SafeLog(logFile, "Deleted temporary solution.");
                    }
                }
                catch (Exception ex)
                {
                    SafeLog(logFile, "Failed to delete temporary solution: " + ex.Message);
                }

                try
                {
                    if (markId != Session.UndoMarkId.Null)
                        session.DeleteUndoMark(markId, null);
                }
                catch
                {
                }
            }
        }

        private static void SafeLog(LogFile logFile, string message)
        {
            try
            {
                if (logFile != null)
                    logFile.WriteLine("[TempSolutionRefresh] " + message);
            }
            catch
            {
            }
        }
    }
}

int status =
    NxSolutionRefreshScope.RunWithClonedTemporarySolution(
        workSimPart,
        theSession,
        theSession.LogFile,
        delegate
        {
            return FPDWriter.WriteFpdFile(
                fpdPath,
                simulationObj,
                mesh);
        });
