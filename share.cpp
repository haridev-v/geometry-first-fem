private BoundaryFaceExport BuildBoundaryFaceExport(
    Dictionary<int, CAEBody> bodyNumbers,
    BoundaryFaceRegistry registry,
    UFSession ufSession,
    Session session)
{
    BoundaryFaceExport export = new BoundaryFaceExport();

    foreach (BoundaryFaceTagEntry faceEntry in registry.GetUniqueMarkerTags())
    {
        int faceMarker = faceEntry.Marker;
        Tag targetLiveTag = faceEntry.LiveTag;
        Tag targetCanonicalTag = faceEntry.CanonicalTag;

        bool found = false;

        foreach (KeyValuePair<int, CAEBody> bodyKvp in bodyNumbers.OrderBy(x => x.Key))
        {
            int domainId = bodyKvp.Key;
            CAEBody body = bodyKvp.Value;

            if (body == null)
                continue;

            int noOfFaces = 0;
            Tag[] bodyFaceTags = null;

            ufSession.Sf.BodyAskFaces(
                body.Tag,
                out noOfFaces,
                out bodyFaceTags);

            if (bodyFaceTags == null || bodyFaceTags.Length == 0)
                continue;

            for (int i = 0; i < bodyFaceTags.Length; i++)
            {
                Tag currentLiveTag = bodyFaceTags[i];

                if (!TagsReferToSameFace(
                        currentLiveTag,
                        targetLiveTag,
                        targetCanonicalTag,
                        session))
                    continue;

                TaggedObject obj =
                    session.GetObjectManager().GetTaggedObject(currentLiveTag);

                CAEFace currentFace = obj as CAEFace;

                string faceName = "";

                if (currentFace != null)
                {
                    try
                    {
                        faceName = currentFace.Name == null
                            ? ""
                            : currentFace.Name.Trim();
                    }
                    catch
                    {
                        faceName = "";
                    }
                }

                export.Faces.Add(new BoundaryFaceRef
                {
                    FaceMarker = faceMarker,
                    DomainId = domainId,
                    FaceOrdinalWithinBody = i + 1,
                    FaceName = faceName
                });

                found = true;
                break;
            }

            if (found)
                break;
        }

        if (!found)
        {
            throw new InvalidOperationException(
                "Could not locate registered boundary face marker " +
                faceMarker + ".");
        }
    }

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

