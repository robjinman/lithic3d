#pragma once

#include <string>
#include <memory>
#include <vector>
#include <ostream>

namespace lithic3d
{

class XmlNode
{
  public:
    class Iterator
    {
      public:
        Iterator(std::vector<std::unique_ptr<XmlNode>>::const_iterator i);

        bool operator==(const Iterator& rhs) const;
        bool operator!=(const Iterator& rhs) const;
        const XmlNode& operator*() const;
        const XmlNode* operator->() const;
        Iterator& operator++();
        void next();

      private:
        std::vector<std::unique_ptr<XmlNode>>::const_iterator m_i;
    };

    virtual std::unique_ptr<XmlNode> clone() const = 0;
    virtual const std::string& name() const = 0;
    virtual const std::string& value() const = 0;
    virtual Iterator child(const std::string& name) const = 0;
    virtual std::string attribute(const std::string& name) const = 0;
    virtual Iterator begin() const = 0;
    virtual Iterator end() const = 0;

    virtual void addChild(XmlNodePtr node) = 0;
    virtual void setValue(const std::string& value) = 0;
    virtual void setAttribute(const std::string& name, const std::string& value) = 0;
    virtual void write(std::ostream& stream) const = 0;

    virtual ~XmlNode() {}
};

using XmlNodePtr = std::unique_ptr<XmlNode>;

XmlNodePtr createXmlNode(const std::string& name);

XmlNodePtr parseXml(const std::vector<char>& data);
XmlNodePtr parseXml(std::string_view data);

} // namespace lithic3d
