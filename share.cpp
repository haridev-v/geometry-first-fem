    return export;
}

private static bool TagsReferToSameFace(
    Tag currentLiveTag,
    Tag targetLiveTag,
    Tag targetCanonicalTag,
    Session session)
{
    if (currentLiveTag == Tag.Null)
        return false;

    if (targetLiveTag != Tag.Null && currentLiveTag == targetLiveTag)
        return true;

    Tag currentCanonicalTag =
        GetCanonicalTagFromTag(currentLiveTag, session);

    if (targetCanonicalTag != Tag.Null &&
        currentCanonicalTag == targetCanonicalTag)
        return true;

    if (targetLiveTag != Tag.Null)
    {
        Tag targetCanonicalFromLive =
            GetCanonicalTagFromTag(targetLiveTag, session);

        if (targetCanonicalFromLive != Tag.Null &&
            currentCanonicalTag == targetCanonicalFromLive)
            return true;
    }

    return false;
}

private static Tag GetCanonicalTagFromTag(Tag tag, Session session)
{
    if (tag == Tag.Null || session == null)
        return Tag.Null;

    try
    {
        TaggedObject obj =
            session.GetObjectManager().GetTaggedObject(tag);

        if (obj == null)
            return tag;

        return NXProgram.GetCanonicalTag(obj);
    }
    catch
    {
        return tag;
    }
}

public static MeshData ExtractMesh(
    FemPart workFemPart,
    CaePart caePart,
    UFSession theUFSession,
    Session theSession,
    Dictionary<int, CAEBody> bodyNumbers,
    List<Tag> boundaryFaceTags,
    Dictionary<Tag, int> markerByFaceTags,
    List<ExportDomainInfo> exportDomains,
    Action<int, string, string> progress)

for (int bf = 0; bf < boundaryFaceTags.Count; bf++)
{
    Tag faceTag = boundaryFaceTags[bf];

    if (faceTag == Tag.Null)
        continue;

    TaggedObject obj =
        theSession.GetObjectManager().GetTaggedObject(faceTag);

    CAEFace caeFace = obj as CAEFace;

    if (caeFace == null)
        continue;

    int faceMarker;

    if (!markerByFaceTags.TryGetValue(faceTag, out faceMarker))
        faceMarker = bf + 1;

    // Existing related-face / facet extraction logic uses caeFace here.
}

var mesh =
    MeshDataExtractor.MeshExtractor.ExtractMesh(
        workFemPart,
        caePart,
        theUFSession,
        theSession,
        NXProgram.bodyNumbers,
        NXProgram.BoundaryFaces.FaceTags,
        NXProgram.BoundaryFaces.MarkerByFaceTags,
        exportDomains,
        (pct, msg1, msg2) =>
        {
            progress.Report(
                (int)(0.80 * pct),
                msg1,
                msg2);
        });

MeshData mesh =
    MeshDataExtractor.MeshExtractor.ExtractMesh(
        workFemPart,
        caePart,
        theUFSession,
        theSession,
        bodyNumbers,
        workerRegistry.FaceTags,
        workerRegistry.MarkerByFaceTags,
        exportDomains,
        (pct, msg1, msg2) =>
        {
            WriteHeartbeat(
                job.HeartbeatPath,
                $"[{pct}%] {msg1} | {msg2}");
        });

Tag liveTag = Tag.Null;
Tag canonicalTag = Tag.Null;

try
{
    liveTag = faceObj.Tag;
}
catch
{
    continue;
}

try
{
    canonicalTag = NXProgram.GetCanonicalTag(faceObj);
}
catch
{
    canonicalTag = liveTag;
}

int faceMarker;

if (!NXProgram.BoundaryFaces.TryGetMarkerByTags(
        liveTag,
        canonicalTag,
        out faceMarker))
{
    NXProgram.logFile.WriteLine(
        "Add BCs: could not resolve face marker. " +
        "liveTag=" + liveTag +
        ", canonicalTag=" + canonicalTag);

    continue;
}

faceMarkers.Add(faceMarker);


int faceStep = 0;
int faceCount = boundaryFaceTags == null ? 0 : boundaryFaceTags.Count;

if (faceCount > 0)
{
    foreach (Tag faceTag in boundaryFaceTags)
    {
        if (faceTag == Tag.Null)
        {
            logFile.WriteLine("Boundary face tag is null.");
            logContents.Add("Boundary face tag is null.");
            continue;
        }

        CAEFace caeFace = ResolveCaeFaceFromTag(faceTag, theSession);

        if (caeFace == null)
        {
            logFile.WriteLine("Unable to resolve CAEFace from tag " + faceTag);
            logContents.Add("Unable to resolve CAEFace from tag " + faceTag);
            continue;
        }

        int faceMarker;

        if (!TryGetBoundaryFaceMarker(
                caeFace,
                faceTag,
                markerByFaceTags,
                out faceMarker))
        {
            logFile.WriteLine(
                "Unable to get face marker for CAEFace tag " + faceTag);

            logContents.Add(
                "Unable to get face marker for CAEFace tag " + faceTag);

            continue;
        }

        faceStep++;

        int pct = 75 + (int)(25.0 * faceStep / Math.Max(faceCount, 1));

        progress?.Invoke(
            pct,
            "Pass 3/3: Start processing facets on face " +
                faceStep + "/" + faceCount,
            "Surface facets = " + faceMarker);

        CAEFace[] oneFace = new CAEFace[] { caeFace };

        RelatedElemFaceMethod relFaceMethod = null;

        try
        {
            relFaceMethod =
                caePart.SmartSelectionMgr.CreateRelatedElemFaceMethodFromFaces(
                    oneFace,
                    false,
                    FacePreference.First2dElse3d,
                    true);

            FEElemFace[] feFaces = relFaceMethod.GetFEElemFaces();

            if (feFaces == null || feFaces.Length == 0)
            {
                logContents.Add(
                    "feFaces is null or empty for CAEFace tag " + faceTag);

                continue;
            }

            foreach (FEElemFace feFace in feFaces)
            {
                FEElement[] elems = null;
                int[] faceIds = null;

                try
                {
                    elems = feFace.GetElementsAndFaceIds(out faceIds);
                }
                catch
                {
                    logContents.Add(
                        "GetElementsAndFaceIds failed for CAEFace tag " +
                        faceTag);

                    continue;
                }

                if (elems == null ||
                    elems.Length == 0 ||
                    faceIds == null ||
                    faceIds.Length == 0)
                {
                    logContents.Add(
                        "Empty arrays for FEFace on CAEFace tag " + faceTag);

                    continue;
                }

                FEElement ownerElem = null;
                int ownerFaceId = -1;
                int ownerDomainId = -1;

                for (int k = 0; k < elems.Length; k++)
                {
                    FEElement elem = elems[k];

                    if (elem == null)
                        continue;

                    if (!elemTagToDomainId.TryGetValue(
                            elem.Tag,
                            out ownerDomainId))
                    {
                        continue;
                    }

                    ownerElem = elem;
                    ownerFaceId = faceIds[k];
                    break;
                }

                if (ownerElem == null || ownerFaceId < 0)
                {
                    logContents.Add(
                        "No exported owner element found for boundary face marker " +
                        faceMarker);

                    continue;
                }

                // Continue with your existing facet-node extraction logic here.
                // Use:
                //     ownerElem
                //     ownerFaceId
                //     ownerDomainId
                //     faceMarker
                //
                // Do NOT use caeFace.Prototype or object equality.
            }
        }
        finally
        {
            ReleaseNxTransient(relFaceMethod);
        }
    }
}

private static CAEFace ResolveCaeFaceFromTag(
    Tag faceTag,
    Session session)
{
    if (faceTag == Tag.Null || session == null)
        return null;

    try
    {
        TaggedObject obj =
            session.GetObjectManager().GetTaggedObject(faceTag);

        return obj as CAEFace;
    }
    catch
    {
        return null;
    }
}

private static bool TryGetBoundaryFaceMarker(
    CAEFace caeFace,
    Tag liveFaceTag,
    IReadOnlyDictionary<Tag, int> markerByFaceTags,
    out int faceMarker)
{
    faceMarker = -1;

    if (markerByFaceTags == null)
        return false;

    if (liveFaceTag != Tag.Null &&
        markerByFaceTags.TryGetValue(liveFaceTag, out faceMarker))
    {
        return true;
    }

    if (caeFace == null)
        return false;

    Tag selectedTag = Tag.Null;
    Tag canonicalTag = Tag.Null;

    try
    {
        selectedTag = caeFace.Tag;
    }
    catch
    {
        selectedTag = liveFaceTag;
    }

    try
    {
        canonicalTag = NXProgram.GetCanonicalTag(caeFace);
    }
    catch
    {
        canonicalTag = selectedTag;
    }

    if (canonicalTag != Tag.Null &&
        markerByFaceTags.TryGetValue(canonicalTag, out faceMarker))
    {
        return true;
    }

    if (selectedTag != Tag.Null &&
        markerByFaceTags.TryGetValue(selectedTag, out faceMarker))
    {
        return true;
    }

    return false;
}

private static void ReleaseNxTransient(object obj)
{
    if (obj == null)
        return;

    try
    {
        IDisposable disposable = obj as IDisposable;

        if (disposable != null)
        {
            disposable.Dispose();
            return;
        }
    }
    catch
    {
    }

    try
    {
        System.Reflection.MethodInfo destroyMethod =
            obj.GetType().GetMethod("Destroy");

        if (destroyMethod != null)
            destroyMethod.Invoke(obj, null);
    }
    catch
    {
    }
}
