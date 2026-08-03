using System;
using System.Collections.Generic;
using NXOpen;
using NXOpen.CAE;

namespace SimDataExtractor.PhysicsDataUtils
{
    public sealed class BoundaryFaceRegistry
    {
        private readonly List<Tag> faceTags =
            new List<Tag>();

        private readonly Dictionary<Tag, int> markerByFaceTag =
            new Dictionary<Tag, int>();

        private readonly Dictionary<int, Tag> liveTagByMarker =
            new Dictionary<int, Tag>();

        private readonly Dictionary<int, Tag> canonicalTagByMarker =
            new Dictionary<int, Tag>();

        public IReadOnlyList<Tag> FaceTags
        {
            get { return faceTags; }
        }

        public IReadOnlyDictionary<Tag, int> MarkerByFaceTags
        {
            get { return markerByFaceTag; }
        }

        public int Register(CAEFace face)
        {
            if (face == null)
                return -1;

            Tag liveTag;
            Tag canonicalTag;

            if (!GetFaceTags(face, out liveTag, out canonicalTag))
                return -1;

            return RegisterTags(liveTag, canonicalTag);
        }

        public int RegisterTags(Tag liveTag, Tag canonicalTag)
        {
            if (liveTag == Tag.Null && canonicalTag == Tag.Null)
                return -1;

            int existingMarker;

            if (TryGetMarkerByTags(liveTag, canonicalTag, out existingMarker))
            {
                RegisterAliases(liveTag, canonicalTag, existingMarker);
                return existingMarker;
            }

            int marker = faceTags.Count + 1;

            RegisterWithMarker(liveTag, canonicalTag, marker);

            return marker;
        }

        public bool TryGetMarker(CAEFace face, out int marker)
        {
            marker = -1;

            if (face == null)
                return false;

            Tag liveTag;
            Tag canonicalTag;

            if (!GetFaceTags(face, out liveTag, out canonicalTag))
                return false;

            return TryGetMarkerByTags(liveTag, canonicalTag, out marker);
        }

        public bool TryGetMarkerByTags(
            Tag liveTag,
            Tag canonicalTag,
            out int marker)
        {
            marker = -1;

            if (canonicalTag != Tag.Null &&
                markerByFaceTag.TryGetValue(canonicalTag, out marker))
                return true;

            if (liveTag != Tag.Null &&
                markerByFaceTag.TryGetValue(liveTag, out marker))
                return true;

            return false;
        }

        public int GetMarkerByFace(CAEFace face)
        {
            int marker;

            if (TryGetMarker(face, out marker))
                return marker;

            return Register(face);
        }

        public List<int> RegisterMany(IEnumerable<CAEFace> inputFaces)
        {
            List<int> markers = new List<int>();

            if (inputFaces == null)
                return markers;

            foreach (CAEFace face in inputFaces)
            {
                int marker = Register(face);

                if (marker > 0)
                    markers.Add(marker);
            }

            return markers;
        }

        public void RegisterWithMarker(CAEFace face, int marker)
        {
            if (face == null || marker <= 0)
                return;

            Tag liveTag;
            Tag canonicalTag;

            if (!GetFaceTags(face, out liveTag, out canonicalTag))
                return;

            RegisterWithMarker(liveTag, canonicalTag, marker);
        }

        public void RegisterWithMarker(
            Tag liveTag,
            Tag canonicalTag,
            int marker)
        {
            if (marker <= 0)
                return;

            if (liveTag == Tag.Null && canonicalTag == Tag.Null)
                return;

            while (faceTags.Count < marker)
                faceTags.Add(Tag.Null);

            /*
             * IMPORTANT:
             * faceTags is the ordered unique list used by mesh extraction.
             * It should store the live/current-session tag when available.
             * Canonical tag is only an alias for matching.
             */
            Tag representativeTag =
                liveTag != Tag.Null ? liveTag : canonicalTag;

            if (faceTags[marker - 1] == Tag.Null)
                faceTags[marker - 1] = representativeTag;

            if (liveTag != Tag.Null)
                liveTagByMarker[marker] = liveTag;

            if (canonicalTag != Tag.Null)
                canonicalTagByMarker[marker] = canonicalTag;

            RegisterAliases(liveTag, canonicalTag, marker);
        }

        private void RegisterAliases(
            Tag liveTag,
            Tag canonicalTag,
            int marker)
        {
            if (liveTag != Tag.Null)
                markerByFaceTag[liveTag] = marker;

            if (canonicalTag != Tag.Null)
                markerByFaceTag[canonicalTag] = marker;
        }

        public List<BoundaryFaceTagEntry> GetUniqueMarkerEntries()
        {
            List<BoundaryFaceTagEntry> entries =
                new List<BoundaryFaceTagEntry>();

            for (int i = 0; i < faceTags.Count; i++)
            {
                int marker = i + 1;

                Tag liveTag = faceTags[i];

                Tag canonicalTag = Tag.Null;
                canonicalTagByMarker.TryGetValue(marker, out canonicalTag);

                if (liveTag == Tag.Null && canonicalTag == Tag.Null)
                    continue;

                BoundaryFaceTagEntry entry = new BoundaryFaceTagEntry();
                entry.Marker = marker;
                entry.LiveTag = liveTag;
                entry.CanonicalTag = canonicalTag;

                entries.Add(entry);
            }

            return entries;
        }

        private bool GetFaceTags(
            CAEFace face,
            out Tag liveTag,
            out Tag canonicalTag)
        {
            liveTag = Tag.Null;
            canonicalTag = Tag.Null;

            if (face == null)
                return false;

            try
            {
                liveTag = face.Tag;
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
                canonicalTag = liveTag;
            }

            if (canonicalTag == Tag.Null)
                canonicalTag = liveTag;

            return liveTag != Tag.Null || canonicalTag != Tag.Null;
        }

        public void Clear()
        {
            faceTags.Clear();
            markerByFaceTag.Clear();
            liveTagByMarker.Clear();
            canonicalTagByMarker.Clear();
        }
    }

    public class BoundaryFaceTagEntry
    {
        public int Marker { get; set; }
        public Tag LiveTag { get; set; }
        public Tag CanonicalTag { get; set; }
    }
}
