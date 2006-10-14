/* 
 *	Copyright (C) 2005-2006 Team MediaPortal
 *  Author: tourettes
 *	http://www.team-mediaportal.com
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *   
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *   
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#pragma warning( disable: 4995 4996 )

#include "SubTransform.h"
#include "SubtitleInputPin.h"
#include "AudioInputPin.h"
#include "PMTInputPin.h"
#include <bdaiface.h>

const DWORD fccUYVY = MAKEFOURCC('U','Y','V','Y');
const DWORD fccYUY2 = MAKEFOURCC('Y','U','Y','2');

const int subtitleSizeInBytes = 720 * 576 * 3;

const LONG subDelay = 520000;

extern void Log(const char *fmt, ...);

// Setup data
const AMOVIESETUP_MEDIATYPE sudPinTypesSubtitle =
{
	&MEDIATYPE_MPEG2_SECTIONS, &MEDIASUBTYPE_DVB_SI 
};

const AMOVIESETUP_MEDIATYPE sudPinTypesOutput =
{	
	&MEDIATYPE_NULL, &MEDIASUBTYPE_NULL
};

const AMOVIESETUP_PIN sudPins[5] =
{
	{
		L"Subtitle",				  // Pin string name
		FALSE,						    // Is it rendered
		FALSE,						    // Is it an output
		FALSE,						    // Allowed none
		FALSE,						    // Likewise many
		&CLSID_NULL,				  // Connects to filter
		L"Subtitle",				  // Connects to pin
		1,							      // Number of types
		&sudPinTypesSubtitle  // Pin information
	},
	{
		L"Video",					  // Pin string name
		FALSE,						  // Is it rendered
		FALSE,						  // Is it an output
		FALSE,						  // Allowed none
		FALSE,						  // Likewise many
		&CLSID_NULL,				// Connects to filter
		L"Video",					  // Connects to pin
		1,							    // Number of types
		&sudPinTypesOutput  // Pin information
	},
	{
		L"Audio",					  // Pin string name
		FALSE,						  // Is it rendered
		FALSE,						  // Is it an output
		FALSE,						  // Allowed none
		FALSE,						  // Likewise many
		&CLSID_NULL,			  // Connects to filter
		L"Audio",					  // Connects to pin
		1,							    // Number of types
		&sudPinTypesOutput	// Pin information
	},
	{
		L"PMT",					    // Pin string name
		FALSE,						  // Is it rendered
		FALSE,						  // Is it an output
		FALSE,						  // Allowed none
		FALSE,						  // Likewise many
		&CLSID_NULL,			  // Connects to filter
		L"PMT",					    // Connects to pin
		1,							    // Number of types
		&sudPinTypesOutput	// Pin information
	},

	{
		L"Output",				  // Pin's string name
		FALSE,						  // Is it rendered
		TRUE,						    // Is it an output
		FALSE,						  // Allowed none
		FALSE,						  // Allowed many
		&CLSID_NULL,			  // Connects to filter
		L"Output",				  // Connects to pin
		1,							    // Number of types
		&sudPinTypesOutput  // Pin type information
	}
};
//
// Constructor
//
CSubTransform::CSubTransform( LPUNKNOWN pUnk, HRESULT *phr ) :
	CTransformFilter( NAME("MediaPortal Sub Transform Filter"), pUnk, CLSID_SubTransform ),
	m_pSubtitlePin( NULL ),
	m_pAudioPin( NULL ),
  m_pPMTPin( NULL ),
	m_pSubDecoder( NULL ),
	m_pSubtitle( NULL ),
	m_bRenderCurrentSubtitle( false ),
	m_curSubtitlePTS( 0 ),
	m_NextSubtitlePTS( 0 ),
	m_curPTS( 0 ),
	m_PTSdiff( 0 ),
	m_firstPTS( 0 ),
	m_firstPTSDone( false ),
	m_bSubtitleDiscarded( true ),
	m_ShowDuration( 388000 ),
	m_pDibBits(NULL),
  m_DibsSub(NULL),
  m_DC(NULL),
  m_OldObject(NULL),
	m_curSubtitleData(NULL)
{
	// Create subtitle decoder
	m_pSubDecoder = new CDVBSubDecoder();
	
	if( m_pSubDecoder == NULL ) 
	{
    if( phr )
		{
      *phr = E_OUTOFMEMORY;
		}
    return;
  }

	// Create subtitle input pin
	m_pSubtitlePin = new CSubtitleInputPin( this,
								GetOwner(),
								this,
								&m_Lock,
								&m_ReceiveLock,
								m_pSubDecoder, 
								phr );
    
	if ( m_pSubtitlePin == NULL ) 
	{
    if( phr )
		{
      *phr = E_OUTOFMEMORY;
		}
      return;
    }

	// Create output pin
	m_pOutput = new CTransformOutputPin(
		NAME("CTransformOutputPin"), this, phr, L"Out" );

	if( m_pOutput == NULL ) 
	{
    if( phr )
		{
      *phr = E_OUTOFMEMORY;
		}
      return;
    }
	
	// Create video input pin
	m_pInput = new CTransformInputPin(
		NAME("CTransformInputPin"), this, phr, L"Video In");

	if ( m_pOutput == NULL ) 
	{
    if( phr )
		{
      *phr = E_OUTOFMEMORY;
		}
      return;
    }
	
	// Create audio input pin
	m_pAudioPin = new CAudioInputPin( this,
								GetOwner(),
								this,
								&m_Lock,
								&m_ReceiveLock,
								phr );

	if ( m_pAudioPin == NULL ) 
	{
    if( phr )
		{
      *phr = E_OUTOFMEMORY;
		}
      return;
    }

	// Create PMT input pin
	m_pPMTPin = new CPMTInputPin( this,
								GetOwner(),
								this,
								&m_Lock,
								&m_ReceiveLock,
								phr,
                this ); // MPidObserver

	if ( m_pPMTPin == NULL ) 
	{
    if( phr )
		{
      *phr = E_OUTOFMEMORY;
		}
      return;
    }

	m_curSubtitleData = NULL;
	m_pSubDecoder->SetObserver( this );
}

CSubTransform::~CSubTransform()
{
	m_pSubDecoder->SetObserver( NULL );
	delete m_pSubDecoder;
	delete m_pSubtitlePin;
	delete m_pAudioPin;
  delete m_pPMTPin;
	DeleteObject( m_DibsSub );
	DeleteObject( m_OldObject );
}

//
// GetPin
//
CBasePin * CSubTransform::GetPin( int n )
{
  if ( n == 0 )
	{
    // CBasePin output pin
		return m_pOutput;	
	}
	if( n == 1 )
	{
		// CBasePin input pin
		return m_pInput;	
	}
	if( n == 2 )
	{
		return m_pSubtitlePin;
	}
	if( n == 3 )
	{
		return m_pAudioPin;
	}
	if( n == 4 )
	{
		return m_pPMTPin;
	}
	else 
	{
    return NULL;
  }
}

int CSubTransform::GetPinCount()
{
	return 5; // input, output, subtitle, audio & PMT
}

HRESULT CSubTransform::CheckInputType( const CMediaType *pmt )
{
  if( IsValidUYVY( pmt ) )
  {
    return S_OK;
  }
  if( IsValidYUY2( pmt ) ) 
	{
    return S_OK;
  } 
	return VFW_E_TYPE_NOT_ACCEPTED;
}

HRESULT CSubTransform::CheckConnect( PIN_DIRECTION dir, IPin *pPin )
{
  HRESULT hr = CTransformFilter::CheckConnect( dir, pPin );

  AM_MEDIA_TYPE mediaType;
  int videoPid = 0;
  
  pPin->ConnectionMediaType( &mediaType );

  // Search for demuxer's video pin
  if(  mediaType.majortype == MEDIATYPE_Video && dir == PINDIR_INPUT )
  {
	  IMPEG2PIDMap* pMuxMapPid;
	  if( SUCCEEDED( pPin->QueryInterface( &pMuxMapPid ) ) )
    {
		  IEnumPIDMap *pIEnumPIDMap;
		  if( SUCCEEDED( pMuxMapPid->EnumPIDMap( &pIEnumPIDMap ) ) )
      {
			  ULONG count = 0;
			  PID_MAP pidMap;
			  while( pIEnumPIDMap->Next( 1, &pidMap, &count ) == S_OK )
        {
          m_VideoPid = pidMap.ulPID;
          m_pPMTPin->SetVideoPid( m_VideoPid );
          Log( "CheckConnect - found video PID %d",  m_VideoPid );
			  }
		  }
		  pMuxMapPid->Release();
    }
  }
  return S_OK;
}


HRESULT CSubTransform::CheckTransform( const CMediaType *mtIn, const CMediaType *mtOut )
{
  // Make sure the subtypes match
  if( mtIn->subtype != mtOut->subtype )
  {
    return VFW_E_TYPE_NOT_ACCEPTED;
  }

  // Currently supported modes are UYVY & YUY2
  if( !IsValidUYVY( mtOut ) )
  {
    if( !IsValidYUY2( mtOut ) ) 
    {
      return VFW_E_TYPE_NOT_ACCEPTED;
    }
  }

  BITMAPINFOHEADER *pBmi = HEADER( mtIn );
  BITMAPINFOHEADER *pBmi2 = HEADER( mtOut );

  if( ( pBmi->biWidth <= pBmi2->biWidth ) &&
      ( pBmi->biHeight == abs( pBmi2->biHeight ) ) )
  {
    return S_OK;
  }
  return VFW_E_TYPE_NOT_ACCEPTED;
}

HRESULT CSubTransform::GetMediaType( int iPosition, CMediaType *pMediaType )
{
  ASSERT( m_pInput->IsConnected() );

  if( iPosition < 0 )
  {
    return E_INVALIDARG;
  }
  else if( iPosition == 0 )
  {
    return m_pInput->ConnectionMediaType( pMediaType );
  }
    
	return VFW_S_NO_MORE_ITEMS;
}

HRESULT CSubTransform::SetMediaType( PIN_DIRECTION direction, const CMediaType *pmt )
{
  if( direction == PINDIR_INPUT )
  {
    ASSERT( pmt->formattype == FORMAT_VideoInfo );
    VIDEOINFOHEADER *pVih = (VIDEOINFOHEADER*)pmt->pbFormat;
    CopyMemory( &m_VihIn, pVih, sizeof( VIDEOINFOHEADER ) );
  }
  else   // Output pin
  {
    ASSERT( direction == PINDIR_OUTPUT );
    ASSERT( pmt->formattype == FORMAT_VideoInfo );
    VIDEOINFOHEADER *pVih = (VIDEOINFOHEADER*)pmt->pbFormat;
    CopyMemory( &m_VihOut, pVih, sizeof( VIDEOINFOHEADER ) );
  }
  return S_OK;
}

HRESULT inline CSubTransform::ProcessFrameUYVY( BYTE *pbInput, BYTE *pbOutput, long *pcbByte )
{
  DWORD dwWidth, dwHeight; 
  DWORD dwWidthOut, dwHeightOut;
  LONG  lStrideIn, lStrideOut;
  BYTE  *pbSource, *pbTarget;

	*pcbByte = m_VihOut.bmiHeader.biSizeImage;

  GetVideoInfoParameters( &m_VihIn, pbInput, &dwWidth, &dwHeight, &lStrideIn, &pbSource, true );
  GetVideoInfoParameters( &m_VihOut, pbOutput, &dwWidthOut, &dwHeightOut, &lStrideOut, &pbTarget, true );

	int moveY = (576 - 480)/2 * 720;
	//long place = moveY*3;
	long place = 0;
	bool firstWord( true );
	WORD pixel; int Y=0, U=0, V=0;

	for( DWORD y = 0 ; y < dwHeight ; y++ )
  {
    WORD *pwTarget = (WORD*)pbTarget;
    WORD *pwSource = (WORD*)pbSource;

    for( DWORD x = 0 ; x < dwWidth ; x++ )
    {
		  Y = m_curSubtitleData[place];
		  U = m_curSubtitleData[place + 1];
		  V = m_curSubtitleData[place + 2];

		  place+=3;

		  if( m_bRenderCurrentSubtitle == false || ( V == 0 && U == 0 ) )
		  {
			  pixel = pwSource[x];
		  }
		  else
		  {
			  pixel = Y * 0x100;

			  if( firstWord )
			  {
				  pixel += U;
				  firstWord = false;
			  }
			  else
			  {
				  pixel += V;
				  firstWord = true;
			  }
		}
		pwTarget[x] = pixel;
  }

  // Advance the stride on both buffers.
  pbTarget += lStrideOut;
  pbSource += lStrideIn;
  }

	return S_OK;
}

HRESULT CSubTransform::ProcessFrameYUY2(  BYTE *pbInput, 
                                          BYTE *pbOutput, 
										                      long *pcbByte )
{
  DWORD dwWidth, dwHeight; 
  DWORD dwWidthOut, dwHeightOut;
  LONG  lStrideIn, lStrideOut;
  BYTE  *pbSource, *pbTarget;

	*pcbByte = m_VihOut.bmiHeader.biSizeImage;

  GetVideoInfoParameters( &m_VihIn, pbInput, &dwWidth, &dwHeight, &lStrideIn, &pbSource, true );
  GetVideoInfoParameters( &m_VihOut, pbOutput, &dwWidthOut, &dwHeightOut, &lStrideOut, &pbTarget, true );

	int moveY = (576 - 480)/2 * 720;
	//long place = moveY*3;
	long place = 0;
	bool firstWord( true );
	WORD pixel; int Y=0, U=0, V=0;

	for( DWORD y = 0 ; y < dwHeight ; y++ )
  {
		WORD *pwTarget = (WORD*)pbTarget;
    WORD *pwSource = (WORD*)pbSource;

     for( DWORD x = 0 ; x < dwWidth ; x++ )
     {
        Y = m_curSubtitleData[place];
        U = m_curSubtitleData[place + 1];
        V = m_curSubtitleData[place + 2];

  			place+=3;

	  		if( m_bRenderCurrentSubtitle == false || ( V == 0 && U == 0 ) )
		  	{
			  	pixel = pwSource[x];
			  }
			  else
			  {
				  pixel = Y;

				  if( firstWord )
				  {
					  pixel += U * 0x100;
					  firstWord = false;
				  }
				  else
				  {
					  pixel += V * 0x100;
					  firstWord = true;
				  }
			  }
			  pwTarget[x] = pixel;
      }

      // Advance the stride on both buffers.
      pbTarget += lStrideOut;
      pbSource += lStrideIn;
    }

  return S_OK;
}

HRESULT CSubTransform::DecideBufferSize( IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pProp )
{
    if( !m_pInput->IsConnected() ) 
    {
        return E_UNEXPECTED;
    }

    ALLOCATOR_PROPERTIES InputProps;

    IMemAllocator *pAllocInput = 0;
    HRESULT hr = m_pInput->GetAllocator( &pAllocInput );

    if( FAILED( hr ) )
    {
        return hr;
    }

    hr = pAllocInput->GetProperties( &InputProps );
    pAllocInput->Release();

    if( FAILED( hr ) ) 
    {
        return hr;
    }

    if( pProp->cbBuffer == 0 )
    {
        pProp->cBuffers = 1;
    }

	if( pProp->cbAlign == 0 )
    {
        pProp->cbAlign = 1;
    }

	pProp->cbBuffer = max( InputProps.cbBuffer, pProp->cbBuffer );

    ALLOCATOR_PROPERTIES Actual;
    hr = pAlloc->SetProperties( pProp, &Actual );
    if( FAILED( hr ) ) 
    {
        return hr;
    }
    
    if( InputProps.cbBuffer > Actual.cbBuffer ) 
    {
        return E_FAIL;
    }
    
    return S_OK;
}
STDMETHODIMP CSubTransform::Run( REFERENCE_TIME tStart )
{
  CAutoLock cObjectLock( m_pLock );
	Reset();
	return CBaseFilter::Run( tStart );
}

STDMETHODIMP CSubTransform::Pause()
{
  CAutoLock cObjectLock( m_pLock );
	//Reset();
	return CBaseFilter::Pause();
}

STDMETHODIMP CSubTransform::Stop()
{
    CAutoLock cObjectLock( m_pLock );
	
	Reset();

	return CBaseFilter::Stop();
}

HRESULT CSubTransform::Transform( IMediaSample *pSource, IMediaSample *pDest )
{   
	CAutoLock cObjectLock( m_pLock );
	HRESULT hr( 0 );
	m_curPTS = m_pAudioPin->GetCurrentPTS();
	
	// Look for format changes from the video renderer.
  CMediaType *pmt = 0;
  if( S_OK == pDest->GetMediaType( (AM_MEDIA_TYPE**)&pmt) && pmt )
  {
    // Notify our own output pin about the new type.
    m_pOutput->SetMediaType( pmt );
    DeleteMediaType( pmt );
  }

  if( m_curSubtitleData == NULL ) 
	{
		m_curSubtitleData = new BYTE[m_VihOut.bmiHeader.biWidth * abs(m_VihOut.bmiHeader.biHeight)* 3];
	}

  BYTE *pBufferIn, *pBufferOut;
  pSource->GetPointer( &pBufferIn );
  pDest->GetPointer( &pBufferOut );

  long cbByte = 0;

	if( m_pSubtitle == NULL )
	{
		m_NextSubtitlePTS = 0;
		
		m_pSubtitle	= m_pSubDecoder->GetSubtitle( 0 );
		if( m_pSubtitle )
		{
			m_curSubtitlePTS = m_pSubtitle->PTS();
			if( m_PTSdiff == 0 )
			{
				Log("first subtitle from decoder ( m_curSubtitlePTS: %lld - m_firstPTS: %lld ) - %lld", m_curSubtitlePTS, m_firstPTS, m_curSubtitlePTS - m_firstPTS );
				//m_PTSdiff = m_curSubtitlePTS - m_firstPTS;
			}
		}

		CSubtitle* pNextSubtitle = m_pSubDecoder->GetSubtitle( 1 );
		if( pNextSubtitle )
		{
			m_NextSubtitlePTS = pNextSubtitle->PTS();
		}
		PTSTime cur;
		PTSToPTSTime(m_curSubtitlePTS,&cur);
		Log("Current PTS: %lld %d:%02d:%02d:%03d", m_curSubtitlePTS,cur.h,cur.m,cur.s,cur.u);
		if( m_curSubtitlePTS > 0 )
		{
			m_curSubtitlePTS += subDelay;   //288000;
		}
		//PTSToPTSTime(m_curSubtitlePTS,&cur);
		//Log("Current PTS fixed: %lld %d:%02d:%02d:%03d", m_curSubtitlePTS,cur.h,cur.m,cur.s,cur.u);

		if( m_NextSubtitlePTS > 0 )
		{
			m_NextSubtitlePTS += subDelay;  //288000;
		}
	}

	// Check subtitle status
	if( m_pSubtitle )
	{
		// Update Subtitle bitmap data
		if( m_bSubtitleDiscarded )
		{
			m_bSubtitleDiscarded = false;
			StretchSubtitle();
		}

		Log("curPTS & subPTS - %lld %lld", m_curPTS, m_curSubtitlePTS );

		//if( ( m_curSubtitlePTS >= m_curPTS ) && ( m_curSubtitlePTS + m_ShowDuration ) <= m_curPTS )
		if( m_curPTS >= m_curSubtitlePTS ) 
		{
			Log("Display subtitle!");
			m_bRenderCurrentSubtitle = true;
		}
		else
		{
			m_bRenderCurrentSubtitle = false;
		}

		bool bDiscardSubtitle = false;
		
		if( ( m_curSubtitlePTS + m_ShowDuration ) < m_curPTS )  
		{
			Log("Subtitle has been shown enough!");
			bDiscardSubtitle = true;
		}
		
		if( ( m_NextSubtitlePTS > 0 ) && ( m_NextSubtitlePTS < m_curPTS ) )
		{
			Log("Next subtitle incoming!");
			bDiscardSubtitle = true;
		}

		// Subtitle can be discarded 
		if( bDiscardSubtitle && 
			!m_bSubtitleDiscarded &&
			( m_pSubtitle != NULL ) ) 	
		{
			Log("Time to discard the current subtitle !");
			m_bSubtitleDiscarded = true;
			m_bRenderCurrentSubtitle = false;
			m_pSubDecoder->ReleaseOldestSubtitle();
			m_pSubtitle	= NULL;
		}
	}

	// Render subtitles & video
	if ( m_VihOut.bmiHeader.biCompression == fccUYVY )
    {
        hr = ProcessFrameUYVY( pBufferIn, pBufferOut, &cbByte );
    }
    else if (m_VihOut.bmiHeader.biCompression == fccYUY2 )
    {
        hr = ProcessFrameYUY2( pBufferIn, pBufferOut, &cbByte );
    }
    else
    {
        return E_UNEXPECTED;
    }

    // Set the size of the destination image.
    ASSERT( pDest->GetSize() >= cbByte );

    pDest->SetActualDataLength( cbByte );

    return hr;
}

void CSubTransform::GetVideoInfoParameters(
    const VIDEOINFOHEADER *pvih, 
    BYTE  * const pbData,        
    DWORD *pdwWidth,			 
    DWORD *pdwHeight,			
    LONG  *plStrideInBytes,		
    BYTE **ppbTop,
    bool bYuv
    )
{
  LONG lStride;

  //  For 'normal' formats, biWidth is in pixels. 
  //  Expand to bytes and round up to a multiple of 4.
  if (pvih->bmiHeader.biBitCount != 0 &&
      0 == (7 & pvih->bmiHeader.biBitCount)) 
  {
    lStride = (pvih->bmiHeader.biWidth * ( pvih->bmiHeader.biBitCount / 8) + 3 ) & ~3;
  } 
  else   // Otherwise, biWidth is in bytes.
  {
    lStride = pvih->bmiHeader.biWidth;
  }

  //  If rcTarget is empty, use the whole image.
  if (IsRectEmpty(&pvih->rcTarget)) 
  {
    *pdwWidth = (DWORD)pvih->bmiHeader.biWidth;
    *pdwHeight = (DWORD)(abs(pvih->bmiHeader.biHeight));
    
    if (pvih->bmiHeader.biHeight < 0 || bYuv)   // Top-down bitmap
    {
      *plStrideInBytes = lStride; // Stride goes "down"
      *ppbTop           = pbData; // Top row is first
    } 
    else  // Bottom-up bitmap
    {
      *plStrideInBytes = -lStride;                    // Stride goes "up"
      *ppbTop = pbData + lStride * ( *pdwHeight - 1 );// Bottom row is first
    }
  } 
  else   // rcTarget is NOT empty. Use a sub-rectangle in the image.
  {
    *pdwWidth = (DWORD)( pvih->rcTarget.right - pvih->rcTarget.left) ;
    *pdwHeight = (DWORD)( pvih->rcTarget.bottom - pvih->rcTarget.top );
    
    if (pvih->bmiHeader.biHeight < 0 || bYuv)   // Top-down bitmap
    {
      // Same stride as above, but first pixel is modified down
      // and over by the target rectangle.
      *plStrideInBytes = lStride;
      *ppbTop = pbData + lStride * pvih->rcTarget.top +
        (pvih->bmiHeader.biBitCount * pvih->rcTarget.left) / 8;
    } 
    else  // Bottom-up bitmap
    {
      *plStrideInBytes = -lStride;
      *ppbTop = pbData + lStride * ( pvih->bmiHeader.biHeight - 
        pvih->rcTarget.top - 1 ) + ( pvih->bmiHeader.biBitCount * pvih->rcTarget.left ) / 8;
    }
  }
}

bool CSubTransform::IsValidUYVY( const CMediaType *pmt )
{
	if( pmt->formattype != FORMAT_VideoInfo )
		return false;

	if(	pmt->majortype != MEDIATYPE_Video )
		return false;

	if( pmt->subtype != MEDIASUBTYPE_UYVY )
		return false;

	return true;
}

bool CSubTransform::IsValidYUY2( const CMediaType *pmt )
{
	if( pmt->formattype != FORMAT_VideoInfo )
		return false;

	if(	pmt->majortype != MEDIATYPE_Video )
		return false;

	if( pmt->subtype != MEDIASUBTYPE_YUY2 )
		return false;

	return true;
}


void CSubTransform::SetAudioPid( LONG pid )
{
  m_pAudioPin->SetAudioPid( pid );
}


void CSubTransform::SetSubtitlePid( LONG pid )
{
  m_pSubtitlePin->SetSubtitlePid( pid );
}

void CSubTransform::Reset()
{
	CAutoLock cObjectLock( m_pLock );

	m_pSubDecoder->Reset();
	m_pSubtitle = NULL;	// NULL the local pointer, as cache is deleted

	m_pSubtitlePin->Reset();

	m_NextSubtitlePTS= 0;
	m_curSubtitlePTS = 0;
	m_curPTS = 0; 
	m_firstPTS = 0;
	m_PTSdiff = 0;
	m_firstPTSDone = false;
	m_bRenderCurrentSubtitle = false;
	m_bSubtitleDiscarded = true;
}

HRESULT CSubTransform::BeginFlush( void )
{
//	Reset();
	return CTransformFilter::BeginFlush();
}
HRESULT CSubTransform::EndFlush( void )
{
	Reset();
	return CTransformFilter::EndFlush();
}

void CSubTransform::Notify()
{
//	Create a fake PTS 
//	CSubtitle* subtitle = m_pSubDecoder->GetLatestSubtitle();
//	subtitle->SetPTS( m_previousFrameTS + 900000 ); // + 900000*25*5 );
//	Log( "fake PTS - %lld ", subtitle->PTS() );
}

//
// Interface methods
//
STDMETHODIMP CSubTransform::SetSubtitlePid( ULONG pPid )
{
	if( m_pSubtitlePin )
	{
		m_pSubtitlePin->SetSubtitlePid( pPid );
	}
	
	return S_OK;
}

STDMETHODIMP CSubTransform::SetAudioPid( ULONG pPid )
{
	if( m_pAudioPin )
	{
		m_pAudioPin->SetAudioPid( pPid );
	}
	
	return S_OK;
}

//
// CreateInstance
//
CUnknown * WINAPI CSubTransform::CreateInstance( LPUNKNOWN punk, HRESULT *phr )
{
  ASSERT( phr );

  CSubTransform *pFilter = new CSubTransform( punk, phr );
  if( pFilter == NULL ) 
  {
    if (phr)
    {
      *phr = E_OUTOFMEMORY;
    }
  }
  return pFilter;
}

void CSubTransform::StretchSubtitle() 
{
	int iRet;
	int iWidth,iHeight;
	LOGFONT LogFont;
	HFONT fonzi;
	PTSTime audio,next,cur;

	if (m_DibsSub == NULL)
	{
		BITMAPINFO bmi;
		ZeroMemory(&bmi,sizeof(BITMAPINFO));
		bmi.bmiHeader.biBitCount = 24;
		bmi.bmiHeader.biHeight = m_VihIn.bmiHeader.biHeight;
		bmi.bmiHeader.biWidth = m_VihIn.bmiHeader.biWidth;
		bmi.bmiHeader.biSizeImage = m_VihIn.bmiHeader.biWidth * abs(m_VihIn.bmiHeader.biHeight) * 3;
		bmi.bmiHeader.biPlanes  = 1;
		bmi.bmiHeader.biCompression	= BI_RGB;
		bmi.bmiHeader.biSize = sizeof(bmi);
    if (bmi.bmiHeader.biHeight>0)
		{
			bmi.bmiHeader.biHeight = -1*bmi.bmiHeader.biHeight;
		}
		m_DibsSub = CreateDIBSection(
									NULL,
									(BITMAPINFO*) &bmi,
									DIB_RGB_COLORS,
									&m_pDibBits,
									NULL,
									0
									);
		
		HDC hdc = GetDC( NULL );
		ASSERT(hdc != 0);
		m_DC = CreateCompatibleDC( hdc );
		ReleaseDC( NULL, hdc );
		m_OldObject = SelectObject( m_DC, m_DibsSub );
	}

	BITMAPINFO bmiSrc;
	memset(&bmiSrc, 0, sizeof(bmiSrc));
	bmiSrc.bmiHeader.biSize				    = sizeof(BITMAPINFOHEADER);
  bmiSrc.bmiHeader.biWidth			    = 720;
  bmiSrc.bmiHeader.biHeight			    = -576;
  bmiSrc.bmiHeader.biPlanes			    = 1;
	bmiSrc.bmiHeader.biBitCount			  = 24;
  bmiSrc.bmiHeader.biCompression		= BI_RGB;
	bmiSrc.bmiHeader.biSizeImage		  = ((m_pSubtitle->m_Bitmap.bmWidth * m_pSubtitle->m_Bitmap.bmBitsPixel + 31) & (~31)) / 8 * m_pSubtitle->m_Bitmap.bmHeight;
	bmiSrc.bmiHeader.biXPelsPerMeter	= 0;
	bmiSrc.bmiHeader.biYPelsPerMeter	= 0;
	bmiSrc.bmiHeader.biClrImportant		= 0;
	bmiSrc.bmiHeader.biClrUsed			  = 0;

	iWidth  = m_VihIn.bmiHeader.biWidth;
  iHeight = abs(m_VihIn.bmiHeader.biHeight);

	BYTE *lpBits = new BYTE[ 720*576*3 ];
	
	memcpy(lpBits,m_pSubtitle->GetData(),720*576*3);
	
	iRet = StretchDIBits(m_DC,
                        // destination rectangle
						            0, 0, iWidth, iHeight,
                        // source rectangle
                        0, 0, 720, 576,
                        (void *) lpBits,
                        &bmiSrc,
                        DIB_RGB_COLORS,
						            SRCCOPY);

	
	SetTextColor(m_DC, RGB(128,128,192));
	SetBkColor(m_DC, RGB(128,128,0));

	CHAR szFontName[32] = "MS Sans serif";

	LogFont.lfStrikeOut = 0;
  LogFont.lfUnderline = 0;
	LogFont.lfWeight = FW_BOLD;
  LogFont.lfHeight = 22;
  LogFont.lfEscapement = 0;
  LogFont.lfItalic = FALSE;
	(void) StringCchCopy((char *)LogFont.lfFaceName,32,(char *)szFontName);
	fonzi = CreateFontIndirect(&LogFont);
	SelectObject(m_DC,fonzi);

	TCHAR szText[256];
	
	PTSToPTSTime(m_curPTS,&audio);
	PTSToPTSTime(m_curSubtitlePTS,&cur);
	PTSToPTSTime(m_NextSubtitlePTS,&next);
	(void)StringCchPrintf( szText, NUMELMS(szText), TEXT("Audio: %d:%02d:%02d:%03d Cur: %d:%02d:%02d:%03d Next: %d:%02d:%02d:%03d"), audio.h,audio.m,audio.s,audio.u,cur.h,cur.m,cur.s,cur.u,next.h,next.m,next.s,next.u);
  BOOL bWorked = TextOut( m_DC, 10, 10, szText, _tcslen( szText ) );

  if (iRet == GDI_ERROR) Log("StrectchDIBits failed");
	else {
		GetBitmapBits( m_DibsSub, iWidth * abs(iHeight)*3, m_pDibBits );
		memcpy( m_curSubtitleData, m_pDibBits, iWidth * iHeight * 3 );
		ZeroMemory( m_pDibBits, iWidth * iHeight * 3 );
	}
}

void CSubTransform::PTSToPTSTime( ULONGLONG pts, PTSTime* ptsTime )
{
	PTSTime time;
	ULONG  _90khz = pts/90;
	time.h=(_90khz/(1000*60*60));
	time.m=(_90khz/(1000*60))-(time.h*60);
	time.s=(_90khz/1000)-(time.h*3600)-(time.m*60);
	time.u=_90khz-(time.h*1000*60*60)-(time.m*1000*60)-(time.s*1000);
	*ptsTime=time;
}
