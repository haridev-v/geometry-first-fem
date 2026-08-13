public static Dictionary<int, CAEBody> RefreshBodyNumbersFromCurrentFem(
    Dictionary<int, CAEBody> oldBodyNumbers,
    FemPart workFemPart,
    LogFile logFile)
{
    Dictionary<int, CAEBody> refreshed =
        new Dictionary<int, CAEBody>();

    if (oldBodyNumbers == null || oldBodyNumbers.Count == 0)
        return refreshed;

    if (workFemPart == null)
        return new Dictionary<int, CAEBody>(oldBodyNumbers);

    CAEBodyCollection femBodies = null;

    try
    {
        femBodies = workFemPart.Bodies;
    }
    catch
    {
        return new Dictionary<int, CAEBody>(oldBodyNumbers);
    }

    List<CAEBody> currentBodies = new List<CAEBody>();

    try
    {
        foreach (CAEBody b in femBodies)
        {
            if (b != null)
                currentBodies.Add(b);
        }
    }
    catch
    {
        return new Dictionary<int, CAEBody>(oldBodyNumbers);
    }

    foreach (KeyValuePair<int, CAEBody> kvp in oldBodyNumbers.OrderBy(x => x.Key))
    {
        int domainId = kvp.Key;
        CAEBody oldBody = kvp.Value;

        if (domainId <= 0 || oldBody == null)
            continue;

        Tag oldLiveTag = SafeTag(oldBody);
        Tag oldCanonicalTag = SafeCanonicalTag(oldBody);

        CAEBody freshBody =
            FindMatchingCurrentBody(
                currentBodies,
                oldLiveTag,
                oldCanonicalTag);

        if (freshBody != null)
        {
            refreshed[domainId] = freshBody;

            try
            {
                if (logFile != null)
                {
                    logFile.WriteLine(
                        "Refreshed CAEBody for domain " + domainId +
                        ". oldTag=" + oldLiveTag +
                        ", oldCanonical=" + oldCanonicalTag +
                        ", freshTag=" + freshBody.Tag +
                        ", freshCanonical=" + SafeCanonicalTag(freshBody));
                }
            }
            catch { }
        }
        else
        {
            // Fallback: keep old body, but log it.
            refreshed[domainId] = oldBody;

            try
            {
                if (logFile != null)
                {
                    logFile.WriteLine(
                        "Could not refresh CAEBody for domain " + domainId +
                        ". Keeping old body. oldTag=" + oldLiveTag +
                        ", oldCanonical=" + oldCanonicalTag);
                }
            }
            catch { }
        }
    }

    return refreshed;
}

private static CAEBody FindMatchingCurrentBody(
    List<CAEBody> currentBodies,
    Tag oldLiveTag,
    Tag oldCanonicalTag)
{
    if (currentBodies == null)
        return null;

    foreach (CAEBody body in currentBodies)
    {
        if (body == null)
            continue;

        Tag liveTag = SafeTag(body);
        Tag canonicalTag = SafeCanonicalTag(body);

        if (oldLiveTag != Tag.Null && liveTag == oldLiveTag)
            return body;

        if (oldCanonicalTag != Tag.Null && canonicalTag == oldCanonicalTag)
            return body;

        if (oldCanonicalTag != Tag.Null && liveTag == oldCanonicalTag)
            return body;

        if (oldLiveTag != Tag.Null && canonicalTag == oldLiveTag)
            return body;
    }

    return null;
}

private static Tag SafeTag(TaggedObject obj)
{
    if (obj == null)
        return Tag.Null;

    try
    {
        return obj.Tag;
    }
    catch
    {
        return Tag.Null;
    }
}

private static Tag SafeCanonicalTag(TaggedObject obj)
{
    if (obj == null)
        return Tag.Null;

    try
    {
        Tag tag = NXProgram.GetCanonicalTag(obj);

        if (tag != Tag.Null)
            return tag;
    }
    catch
    {
    }

    return SafeTag(obj);
}

public static bool IsLiveUsableCaeBody(
    CAEBody body,
    UFSession ufSession,
    LogFile logFile,
    int domainId)
{
    if (body == null || ufSession == null)
        return false;

    Tag bodyTag = SafeTag(body);

    if (bodyTag == Tag.Null)
        return false;

    try
    {
        bool isSheet = body.IsSheetBody;
    }
    catch
    {
        try
        {
            if (logFile != null)
                logFile.WriteLine("Body.IsSheetBody failed for domain " + domainId);
        }
        catch { }

        return false;
    }

    try
    {
        int noOfFaces = 0;
        Tag[] faceTags = null;

        ufSession.Sf.BodyAskFaces(
            bodyTag,
            out noOfFaces,
            out faceTags);

        if (faceTags == null || faceTags.Length == 0)
        {
            try
            {
                if (logFile != null)
                    logFile.WriteLine("BodyAskFaces returned no faces for domain " + domainId);
            }
            catch { }

            return false;
        }

        return true;
    }
    catch (Exception ex)
    {
        try
        {
            if (logFile != null)
            {
                logFile.WriteLine(
                    "BodyAskFaces failed for domain " + domainId +
                    ", tag=" + bodyTag +
                    ", error=" + ex.Message);
            }
        }
        catch { }

        return false;
    }
}

NXRefreshUtils.ForceUpdateAndTouchSimulation(
    NXProgram.theSession,
    NXProgram.workSimPart,
    NXProgram.logFile,
    "Before FPD body refresh");

Dictionary<int, CAEBody> freshBodyNumbers =
    ExportDomainInfo.RefreshBodyNumbersFromCurrentFem(
        NXProgram.bodyNumbers,
        NXProgram.workFemPart,
        NXProgram.logFile);

// Optional but useful: replace global dictionary too.
NXProgram.bodyNumbers = freshBodyNumbers;

// Rebuild body-domain tag aliases.
try
{
    NXProgram.BodyDomains.Clear();

    foreach (KeyValuePair<int, CAEBody> kvp in NXProgram.bodyNumbers)
    {
        NXProgram.BodyDomains.RegisterDomainBody(kvp.Key, kvp.Value);
    }
}
catch { }

exportDomains = ExportDomainInfo.BuildExportDomainPlan(
    freshBodyNumbers,
    NXProgram.caePart,
    NXProgram.logFile);
