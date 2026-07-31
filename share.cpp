public int Register(CAEFace face)
{
    if (face == null)
        return -1;

    Tag selectedTag = Tag.Null;
    Tag canonicalTag = Tag.Null;

    try
    {
        selectedTag = face.Tag;
    }
    catch
    {
        return -1;
    }

    try
    {
        canonicalTag = NXProgram.GetCanonicalTag(face);
    }
    catch
    {
        canonicalTag = selectedTag;
    }

    int existingMarker;

    if (canonicalTag != Tag.Null &&
        markerByFaceTag.TryGetValue(canonicalTag, out existingMarker))
    {
        // Also add selected tag as alias to same marker.
        if (selectedTag != Tag.Null &&
            !markerByFaceTag.ContainsKey(selectedTag))
        {
            markerByFaceTag[selectedTag] = existingMarker;
        }

        return existingMarker;
    }

    if (selectedTag != Tag.Null &&
        markerByFaceTag.TryGetValue(selectedTag, out existingMarker))
    {
        // Also add canonical tag as alias to same marker.
        if (canonicalTag != Tag.Null &&
            !markerByFaceTag.ContainsKey(canonicalTag))
        {
            markerByFaceTag[canonicalTag] = existingMarker;
        }

        return existingMarker;
    }

    int marker = faceTags.Count + 1;

    // Store one representative tag in the ordered marker list.
    // Prefer canonical if available.
    Tag representativeTag =
        canonicalTag != Tag.Null ? canonicalTag : selectedTag;

    if (representativeTag == Tag.Null)
        return -1;

    faceTags.Add(representativeTag);

    // Register both aliases to the same marker.
    if (canonicalTag != Tag.Null &&
        !markerByFaceTag.ContainsKey(canonicalTag))
    {
        markerByFaceTag[canonicalTag] = marker;
    }

    if (selectedTag != Tag.Null &&
        !markerByFaceTag.ContainsKey(selectedTag))
    {
        markerByFaceTag[selectedTag] = marker;
    }

    return marker;
}

public bool TryGetMarker(CAEFace face, out int marker)
{
    marker = -1;

    if (face == null)
        return false;

    Tag selectedTag = Tag.Null;
    Tag canonicalTag = Tag.Null;

    try
    {
        selectedTag = face.Tag;
    }
    catch
    {
        return false;
    }

    try
    {
        canonicalTag = NXProgram.GetCanonicalTag(face);
    }
    catch
    {
        canonicalTag = selectedTag;
    }

    if (canonicalTag != Tag.Null &&
        markerByFaceTag.TryGetValue(canonicalTag, out marker))
        return true;

    if (selectedTag != Tag.Null &&
        markerByFaceTag.TryGetValue(selectedTag, out marker))
        return true;

    return false;
}

