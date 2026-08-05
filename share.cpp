using System;
using System.Collections.Generic;
using NXOpen;
using NXOpen.CAE;

namespace SimDataExtractor
{
    public sealed class CaeBodyDomainRegistry
    {
        private readonly Dictionary<Tag, int> domainIdByBodyTag =
            new Dictionary<Tag, int>();

        private readonly Dictionary<int, Tag> liveTagByDomainId =
            new Dictionary<int, Tag>();

        private readonly Dictionary<int, Tag> canonicalTagByDomainId =
            new Dictionary<int, Tag>();

        public void Clear()
        {
            domainIdByBodyTag.Clear();
            liveTagByDomainId.Clear();
            canonicalTagByDomainId.Clear();
        }

        public void RegisterDomainBody(int originalDomainId, CAEBody body)
        {
            if (originalDomainId <= 0 || body == null)
                return;

            Tag liveTag;
            Tag canonicalTag;

            if (!GetBodyTags(body, out liveTag, out canonicalTag))
                return;

            RegisterDomainBodyTags(originalDomainId, liveTag, canonicalTag);
        }

        public void RegisterDomainBodyTags(
            int originalDomainId,
            Tag liveTag,
            Tag canonicalTag)
        {
            if (originalDomainId <= 0)
                return;

            if (liveTag == Tag.Null && canonicalTag == Tag.Null)
                return;

            if (liveTag != Tag.Null)
            {
                domainIdByBodyTag[liveTag] = originalDomainId;
                liveTagByDomainId[originalDomainId] = liveTag;
            }

            if (canonicalTag != Tag.Null)
            {
                domainIdByBodyTag[canonicalTag] = originalDomainId;
                canonicalTagByDomainId[originalDomainId] = canonicalTag;
            }
        }

        public bool TryGetOriginalDomainId(
            CAEBody body,
            out int originalDomainId)
        {
            originalDomainId = -1;

            if (body == null)
                return false;

            Tag liveTag;
            Tag canonicalTag;

            if (!GetBodyTags(body, out liveTag, out canonicalTag))
                return false;

            return TryGetOriginalDomainIdByTags(
                liveTag,
                canonicalTag,
                out originalDomainId);
        }

        public bool TryGetOriginalDomainIdByTags(
            Tag liveTag,
            Tag canonicalTag,
            out int originalDomainId)
        {
            originalDomainId = -1;

            if (canonicalTag != Tag.Null &&
                domainIdByBodyTag.TryGetValue(canonicalTag, out originalDomainId))
                return true;

            if (liveTag != Tag.Null &&
                domainIdByBodyTag.TryGetValue(liveTag, out originalDomainId))
                return true;

            return false;
        }

        private bool GetBodyTags(
            CAEBody body,
            out Tag liveTag,
            out Tag canonicalTag)
        {
            liveTag = Tag.Null;
            canonicalTag = Tag.Null;

            if (body == null)
                return false;

            try
            {
                liveTag = body.Tag;
            }
            catch
            {
                return false;
            }

            try
            {
                canonicalTag = NXProgram.GetCanonicalTag(body);
            }
            catch
            {
                canonicalTag = liveTag;
            }

            if (canonicalTag == Tag.Null)
                canonicalTag = liveTag;

            return liveTag != Tag.Null || canonicalTag != Tag.Null;
        }
    }
}

public static CaeBodyDomainRegistry BodyDomains =
    new CaeBodyDomainRegistry();

NXProgram.BodyDomains.RegisterDomainBody(domainId, body);

CAEBody bodyObj = setObject.Obj as CAEBody;

if (bodyObj == null)
    continue;

int originalDomainId;

if (!NXProgram.BodyDomains.TryGetOriginalDomainId(
        bodyObj,
        out originalDomainId))
{
    NXProgram.logFile.WriteLine(
        "Add BCs: could not resolve CAEBody to domain. bodyTag=" +
        SafeTagString(bodyObj));

    continue;
}

bodyDomainIds.Add(originalDomainId);

private static string SafeTagString(TaggedObject obj)
{
    if (obj == null)
        return "";

    try
    {
        return obj.Tag.ToString();
    }
    catch
    {
        return "";
    }
}
