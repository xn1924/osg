/* -*-c++-*- OpenSceneGraph - Copyright (C) 1999-2008 Robert Osfield
 *
 * This software is open source and may be redistributed and/or modified under
 * the terms of the GNU General Public License (GPL) version 2.0.
 * The full license is in LICENSE.txt file included with this distribution,.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * include LICENSE.txt for more details.
*/

#include <osgDB/ReaderWriter>
#include <osgDB/FileNameUtils>
#include <osgDB/Registry>
#include <osgDB/FileUtils>

#include <osgWidget/PdfReader>
#include <osg/ImageUtils>

#include <ApplicationServices/ApplicationServices.h>



class CGPDFDocumentImage : public osgWidget::PdfImage
{
    public:

        CGPDFDocumentImage():
            osgWidget::PdfImage(),
            _doc(NULL)
        {
            
        }

        virtual ~CGPDFDocumentImage()
        {
            if (_doc)
                CGPDFDocumentRelease(_doc);
            _doc = NULL;
        }

        int getNumOfPages() { return (_doc) ? CGPDFDocumentGetNumberOfPages(_doc) : 0;  }

        bool open(const std::string& filename)
        {
            OSG_NOTICE<<"open("<<filename<<")"<<std::endl;

            std::string foundFile = osgDB::findDataFile(filename);
            if (foundFile.empty())
            {
                OSG_NOTICE<<"could not find filename="<<filename<<std::endl;
                return false;
            }

            
            _pageNum = 0;
            setFileName(filename);
            
            CFStringRef cf_string = CFStringCreateWithCString(NULL, filename.c_str(), kCFStringEncodingUTF8);
            if(!cf_string)
            {
                return false;
            }

            /* Create a CFURL from a CFString */
            CFURLRef the_url = CFURLCreateWithFileSystemPath(
                NULL,
                cf_string,
                kCFURLPOSIXPathStyle,
                false
            );

            /* Don't need the CFString any more (error or not) */
            CFRelease(cf_string);
            
            
            _doc = CGPDFDocumentCreateWithURL(the_url);
            CFRelease(the_url);

            OSG_NOTICE<<"getNumOfPages()=="<<getNumOfPages()<<std::endl;

            if (getNumOfPages()==0)
            {
                return false;
            }

            page(0);

            return true;
        }

        virtual bool sendKeyEvent(int key, bool keyDown)
        {
            if (keyDown && key!=0)
            {
                if (key==_nextPageKeyEvent)
                {
                    next();
                    return true;
                }
                else if (key==_previousPageKeyEvent)
                {
                    previous();
                    return true;
                }
            }
            return false;
        }


        virtual bool page(int pageNum)
        {
            static const float scale = 2.0;
            CGPDFPageRef page = CGPDFDocumentGetPage(_doc, pageNum + 1);
            CGRect mediaRect = CGPDFPageGetBoxRect(page, kCGPDFCropBox);
            
            allocateImage(mediaRect.size.width * scale, mediaRect.size.height * scale, 1, GL_RGBA, GL_UNSIGNED_BYTE);
            setPixelFormat(GL_RGBA);
            setDataVariance(osg::Object::DYNAMIC);
            setOrigin(osg::Image::TOP_LEFT);
            
            CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
            CGContextRef ctx = CGBitmapContextCreate ( data(),
                 s(),
                 t(),
                 8,
                 getRowSizeInBytes(),
                 colorSpace,
                 kCGImageAlphaPremultipliedLast|kCGBitmapByteOrderDefault);

            
            CGContextScaleCTM(ctx, scale, scale);
            CGContextTranslateCTM(ctx, -mediaRect.origin.x, -mediaRect.origin.y);
            
            CGContextSetRGBFillColor(ctx, _backgroundColor[0], _backgroundColor[1], _backgroundColor[2], _backgroundColor[3]);
            CGContextFillRect(ctx, CGContextGetClipBoundingBox(ctx));
    
            CGContextDrawPDFPage(ctx, page);
            
            CGColorSpaceRelease( colorSpace );
            CGContextRelease(ctx);
            
            dirty();
            return true;
        }


    protected:
        CGPDFDocumentRef _doc;

};


class ReaderWriterPDF : public osgDB::ReaderWriter
{
    public:

        ReaderWriterPDF()
        {
            supportsExtension("pdf","PDF plugin");
        }

        virtual const char* className() const { return "PDF plugin"; }

        virtual osgDB::ReaderWriter::ReadResult readObject(const std::string& file, const osgDB::ReaderWriter::Options* options) const
        {
            return readImage(file,options);
        }

        virtual osgDB::ReaderWriter::ReadResult readImage(const std::string& fileName, const osgDB::ReaderWriter::Options* options) const
        {
            if (!osgDB::equalCaseInsensitive(osgDB::getFileExtension(fileName),"pdf"))
            {
                return osgDB::ReaderWriter::ReadResult::FILE_NOT_HANDLED;
            }

            std::string file = osgDB::findDataFile(fileName);
            if (file.empty())
            {
                return osgDB::ReaderWriter::ReadResult::FILE_NOT_FOUND;
            }

            osg::ref_ptr<CGPDFDocumentImage> image = new CGPDFDocumentImage;
            image->setDataVariance(osg::Object::DYNAMIC);

            image->setOrigin(osg::Image::TOP_LEFT);

            if (!image->open(file))
            {
                return "Could not open "+file;
            }

            return image.get();
        }

        virtual osgDB::ReaderWriter::ReadResult readNode(const std::string& fileName, const osgDB::ReaderWriter::Options* options) const
        {
            osgDB::ReaderWriter::ReadResult result = readImage(fileName, options);
            if (!result.validImage()) return result;


            osg::ref_ptr<osgWidget::PdfReader> pdfReader = new osgWidget::PdfReader();
            if (pdfReader->assign(dynamic_cast<osgWidget::PdfImage*>(result.getImage())))
            {
                return pdfReader.release();
            }
            else
            {
                return osgDB::ReaderWriter::ReadResult::FILE_NOT_HANDLED;
            }
        }
};

// now register with Registry to instantiate the above
// reader/writer.
REGISTER_OSGPLUGIN(pdf, ReaderWriterPDF)

