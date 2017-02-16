/*
 * Serializer.cpp
 *
 * Este archivo intenta reunir todos los elementos necesarios para la serialización del mapa de orb-slam2.
 * No todo el código está aquí, es necesario modificar las declaraciones de las clases KeyFrame, Map y MapPoint
 * agregando declaraciones para constructor por defecto, método serialize y clases friend.
 * En la MapPoint.h se debe agregar dentro de la declaración de la clase:
 *
  	MapPoint();
	friend class boost::serialization::access;
	friend class Serializer;
	template<class Archivo> void serialize(Archivo&, const unsigned int);
 *
 * En KeyFrame.h se debe agregar dentro de la declaración de la clase:
 *
  	KeyFrame();
    friend class boost::serialization::access;
	friend class Serializer;
	template<class Archivo> void serialize(Archivo&, const unsigned int);
 *
 * En Map.h se debe agregar dentro de la declaración de la clase:
 *
	friend class Serializer;
 *
 * Map no requiere constructor por defecto.
 *
 * Finalmente, en main.cc o en cualquier otro lugar se debe crear una única instancia de Serializer, pasando el sistema de orb-slam2 como argumento.
 *
 *
 * Partes de este archivo
 *
 * 1- includes
 * 2- defines: macros para la expansión de boost::serialization
 * 3- Serialize de OpenCV: KeyPoint y Map
 * 4- MapPoint: constructor por defecto y definición de MapPoint::serialize
 * 5- KeyFrame: constructor por defecto y definición de KeyFrame::serialize
 * 6- Serializer: definición de la clase.
 *
 *  Created on: 6/1/2017
 *      Author: alejandro
 */

#include <typeinfo>

// boost::serialization
#include <boost/serialization/serialization.hpp>
#include <boost/archive/tmpdir.hpp>

#include <boost/serialization/base_object.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/list.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/split_member.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/assume_abstract.hpp>

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>

#include <typeinfo>

// Clases de orb-slam2
#include "Serializer.h"
#include "Map.h"	// Incluye MapPoint y KeyFrame, éste incluye a KeyFrameDatabase
#include "KeyFrame.h"
#include "KeyFrameDatabase.h"
#include "MapPoint.h"
#include "System.h"
#include "Tracking.h"


// Defines
#define TIPOS_ARCHIVOS(FORMATO) \
	typedef boost::archive::FORMATO##_iarchive ArchivoEntrada;\
	typedef boost::archive::FORMATO##_oarchive ArchivoSalida;

// Aquí se define el tipo de archivo: binary, text, xml...
TIPOS_ARCHIVOS(binary)

/**
 * La bifurcación consiste en distinguir si serializa carga o guarda.  Se activa definiendo BIFURCACION, pero requiere c++11 para typeinfo
 * En un método serialize(Archivo...) la macro GUARDANDO(Archivo) devuelve true si se está guardando, false si se está cargando.
 *
 * Comentar la siguiente línea para desactivar la capacidad de bifurcación
 */
#define CARGANDO(ARCHIVO) ( typeid(ARCHIVO) == typeid(ArchivoEntrada) )

/** Instanciación explícita*/
#define INST_EXP(CLASE)\
template void CLASE::serialize<ArchivoEntrada>(ArchivoEntrada&,const unsigned int);\
template void CLASE::serialize<ArchivoSalida>(ArchivoSalida&,const unsigned int);




/**
 * OpencCV KeyPoint y Mat
 * Copiado de https://github.com/MathewDenny/ORB_SLAM2 MapPoint.h
 */

// Serialización no intrusiva fuera de la clase: Mat, KeyPoint
BOOST_SERIALIZATION_SPLIT_FREE(::cv::Mat)
namespace boost{namespace serialization{


// KeyPoint
template<class Archive> void serialize(Archive & ar, cv::KeyPoint& kf, const unsigned int version){
	string marca = " KP ";
	ar & marca;
	ar & kf.pt.x;
	ar & kf.pt.y;
	ar & kf.angle;	// Usado en matcher
	ar & kf.octave;	// Usado en LocalMapping y otros
}

// Mat: save
template<class Archive> void save(Archive & ar, const ::cv::Mat& m, const unsigned int version){
	size_t elem_size = m.elemSize();
	int elem_type = m.type();

	ar << m.cols;
	ar << m.rows;
	ar << elem_size;
	ar << elem_type;

	size_t data_size = m.cols * m.rows * elem_size;
	ar << boost::serialization::make_array(m.ptr(), data_size);
}

// Mat: load
template<class Archive> void load(Archive & ar, ::cv::Mat& m, const unsigned int version){
	int cols, rows, elem_type;
	size_t elem_size;

	ar >> cols;
	ar >> rows;
	ar >> elem_size;
	ar >> elem_type;

	m.create(rows, cols, elem_type);

	size_t data_size = m.cols * m.rows * elem_size;
	ar >> boost::serialization::make_array(m.ptr(), data_size);
}


}}

extern ORB_SLAM2::System *Sistema;


// Definiciones de los métodos serialize de las clases MapPoint y KeyFrame
namespace ORB_SLAM2{

//extern System &Sistema;

// MapPoint: usando map ========================================================

/**
 * Constructor por defecto de MapPoint para el serializador.
 * Se encarga de inicializar las variables const, para que el compilador no chille.
 */
MapPoint::MapPoint():
nObs(0), mnTrackReferenceForFrame(0),
mnLastFrameSeen(0), mnBALocalForKF(0), mnFuseCandidateForKF(0), mnLoopPointForKF(0), mnCorrectedByKF(0),
mnCorrectedReference(0), mnBAGlobalForKF(0),mnVisible(1), mnFound(1), mbBad(false),
mpReplaced(static_cast<MapPoint*>(NULL)), mfMinDistance(0), mfMaxDistance(0), mpMap(Sistema->mpMap)//Map::mpMap)
{}

/**
 * Serializador de MapPoint
 * Se invoca al serializar Map::mspMapPoints y KeyFrame::mvpMapPoints, cuyos mapPoints nunca tienen mbBad true.
 * La serialización de MapPoint evita punteros para asegurar el guardado consecutivo de todos los puntos antes de proceder con los KeyFrames.
 * Esto evita problemas no identificados con boost::serialization, que cuelgan la aplicación al guardar.
 */
template<class Archivo> void MapPoint::serialize(Archivo& ar, const unsigned int version){
	// Propiedades
	ar & mnId;
	ar & mnFirstKFid;
	ar & mnVisible;
	ar & mnFound;
	//ar & rgb;	// ¿Serializa cv::Vec3b?

	// Mat
	ar & mWorldPos;
	ar & mDescriptor;	// Reconstruíble, pero con mucho trabajo
}
INST_EXP(MapPoint)


// KeyFrame ========================================================
/**
 * Constructor por defecto para KeyFrame
 * Se ocupa de inicializar los atributos const, para que el compilador no chille.
 * Entre ellos inicializa los atributos no serializables (todos punteros a singletons).
 * Luego serialize se encarga de cambiarle los valores, aunque sean const.
 */
KeyFrame::KeyFrame():
	// Públicas
    mnFrameId(0),  mTimeStamp(0.0), mnGridCols(FRAME_GRID_COLS), mnGridRows(FRAME_GRID_ROWS),
    mfGridElementWidthInv(Frame::mfGridElementWidthInv),
    mfGridElementHeightInv(Frame::mfGridElementHeightInv),

    mnTrackReferenceForFrame(0), mnFuseTargetForKF(0), mnBALocalForKF(0), mnBAFixedForKF(0),
    mnLoopQuery(0), mnLoopWords(0), mnRelocQuery(0), mnRelocWords(0), mnBAGlobalForKF(0),

    fx(Frame::fx), fy(Frame::fy), cx(Frame::cx), cy(Frame::cy), invfx(Frame::invfx), invfy(Frame::invfy),
    mbf(0.0), mb(0.0), mThDepth(0.0),	// Valores no usados en monocular, que pasan por varios constructores.
    N(0), mnScaleLevels(Sistema->mpTracker->mCurrentFrame.mnScaleLevels),
    mfScaleFactor(Sistema->mpTracker->mCurrentFrame.mfScaleFactor),
    mfLogScaleFactor(Sistema->mpTracker->mCurrentFrame.mfLogScaleFactor),
    mvScaleFactors(Sistema->mpTracker->mCurrentFrame.mvScaleFactors),
    mvLevelSigma2(Sistema->mpTracker->mCurrentFrame.mvLevelSigma2),
    mvInvLevelSigma2(Sistema->mpTracker->mCurrentFrame.mvInvLevelSigma2),
    mnMinX(Frame::mnMinX), mnMinY(Frame::mnMinY), mnMaxX(Frame::mnMaxX), mnMaxY(Frame::mnMaxY),
    mK(Sistema->mpTracker->mCurrentFrame.mK),
    // Protegidas:
    mpKeyFrameDB(Sistema->mpKeyFrameDatabase),
    mpORBvocabulary(Sistema->mpVocabulary),
    mbFirstConnection(true),
    mpMap(Sistema->mpMap)
{}

void KeyFrame::buildObservations(){
	// Recorre los puntos
	size_t largo = mvpMapPoints.size();
	for(size_t i=0; i<largo; i++){
		MapPoint *pMP = mvpMapPoints[i];
		if (pMP)
			pMP->AddObservation(this, i);
	}
}

/**
 * Serializador para KeyFrame.
 * Invocado al serializar Map::mspKeyFrames.
 * No guarda mpKeyFrameDB, que se debe asignar de otro modo.
 *
 */
template<class Archive> void KeyFrame::serialize(Archive& ar, const unsigned int version){
	// Preparación para guardar
	if(!CARGANDO(ar)){
		// Recalcula mbNotErase para evitar valores true efímeros
		mbNotErase = !mspLoopEdges.empty();
	}

	//cout << "KeyFrame ";
	ar & mnId;
	//cout << mnId << endl;

	ar & mbNotErase;


	// Mat
	ar & Tcw;	//ar & const_cast<cv::Mat &> (Tcw);
	ar & const_cast<cv::Mat &> (mK);	// Mismo valor en todos los keyframes
	ar & const_cast<cv::Mat &> (mDescriptors);

	// Contenedores
	ar & const_cast<std::vector<cv::KeyPoint> &> (mvKeysUn);
	ar & mvpMapPoints;
	if(mbNotErase)
		ar & mspLoopEdges;


	// Punteros
	ar & mpParent;


	// Sólo load
	if(CARGANDO(ar)){
		// Reconstrucciones
		//int n = const_cast<int &> (N);
		const_cast<int &> (N) = mvKeysUn.size();

		mbNotErase = !mspLoopEdges.empty();

		ComputeBoW();	// Sólo actúa al cargar, porque si el keyframe ya tiene los datos no hace nada.
		SetPose(Tcw);
		// UpdateConnections sólo se puede invocar luego de cargados todos los keyframes, para generar mConnectedKeyFrameWeights, mvpOrderedConnectedKeyFrames, mvOrderedWeights y msChildrens


		// Reconstruir la grilla

		// Dimensiona los vectores por exceso
		std::vector<std::size_t> grid[FRAME_GRID_COLS][FRAME_GRID_ROWS];
		int nReserve = 0.5f*N/(FRAME_GRID_COLS*FRAME_GRID_ROWS);
		for(unsigned int i=0; i<FRAME_GRID_COLS;i++)
			for (unsigned int j=0; j<FRAME_GRID_ROWS;j++)
				grid[i][j].reserve(nReserve);

		for(int i=0;i<N;i++){
			const cv::KeyPoint &kp = mvKeysUn[i];
			int posX = round((kp.pt.x-mnMinX)*mfGridElementWidthInv);
			int posY = round((kp.pt.y-mnMinY)*mfGridElementHeightInv);

			//Keypoint's coordinates are undistorted, which could cause to go out of the image
			if(!(posX<0 || posX>=FRAME_GRID_COLS || posY<0 || posY>=FRAME_GRID_ROWS))
				grid[posX][posY].push_back(i);
		}

		// Copia al vector final.  No sé si esta parte agrega valor.
		mGrid.resize(mnGridCols);
		for(int i=0; i<mnGridCols;i++){
			mGrid[i].resize(mnGridRows);
			for(int j=0; j<mnGridRows; j++)
				mGrid[i][j] = grid[i][j];
		}
	}
}
INST_EXP(KeyFrame)







/**
 * Clase encargada de la serialización (guarda y carga) del mapa->
 */

Serializer::Serializer(Map *mapa_):mapa(mapa_){}

template<class Archivo> void Serializer::serialize(Archivo& ar, const unsigned int version){
	string marca = "";
	//ar & marca;

	// Propiedades
	ar & mapa->mnMaxKFid;

	// Contenedores
	ar & mapa->mspMapPoints; cout << "MapPoints serializados." << endl;
	ar & mapa->mspKeyFrames; cout << "Keyframes serializados." << endl;
	ar & mapa->mvpKeyFrameOrigins;
}


void Serializer::mapSave(char* archivo){
	// Elimina keyframes y mappoint malos de KeyFrame::mvpMapPoints y MapPoint::mObservations
	depurar();

	// Previene la modificación del mapa por otros hilos
	unique_lock<mutex> lock(mapa->mMutexMap);

	// Abre el archivo
	std::ofstream os(archivo);
	ArchivoSalida ar(os, boost::archive::no_header);

	cout << "MapPoints: " << mapa->mspMapPoints.size() << endl
		 << "KeyFrames: " << mapa->mspKeyFrames.size() << endl;
	cout << "Último keyframe: " << mapa->mnMaxKFid << endl;

	// Guarda mappoints y keyframes
	serialize<ArchivoSalida>(ar, 0);
}

/**
 * Carga el mapa desde el archivo binario.
 *
 * @param archivo es el nombre del archivo binario a abrir.
 *
 * Abre el archivo binario y carga su contenido.
 *
 * Luego reconstruye propiedades y grafos necesarios.
 *
 * Omite propiedades efímeras, limitándose a inicializarlas en el constructor por defecto.
 *
 * Es necesario asegurar que otros hilos no modifiquen el mapa ni sus elementos (keyframes y mappoints) durante la carga,
 * pues pueden corromper los datos en memoria, con alta probabilidad de abortar la aplicación por Seg Fault.
 */
void Serializer::mapLoad(char* archivo){
	// A este punto se llega con el mapa limpio y los sistemas suspendidos para no interferir.

	{
		// Por las dudas, se previene la modificación del mapa mientras se carga.  Se libera para la reconstrucción, por las dudas.
		unique_lock<mutex> lock(mapa->mMutexMap);

		// Abre el archivo
		std::ifstream is(archivo);
		ArchivoEntrada ar(is, boost::archive::no_header);

		// Carga mappoints y keyframes en Map
		serialize<ArchivoEntrada>(ar, 0);
	}



	// Reconstrucción, ya cargados todos los keyframes y mappoints.

	KeyFrame::nNextId = mapa->mnMaxKFid + 1;	// mnMaxKFid es el id del último keyframe agregado al mapa

	/*
	 * Recorre los keyframes del mapa:
	 * - Reconstruye la base de datos agregándolos
	 * - UpdateConnections para reconstruir los grafos
	 * - MapPoint::AddObservation sobre cada punto, para reconstruir mObservations y mObs
	 */
	KeyFrameDatabase* kfdb = Sistema->mpKeyFrameDatabase;//Map::mpKeyFrameDatabase;
	for(KeyFrame *pKF : mapa->mspKeyFrames){
		// Agrega el keyframe a la base de datos de keyframes
		kfdb->add(pKF);

		// UpdateConnections para reconstruir el grafo de covisibilidad.  Conviene ejecutarlo en orden de mnId.
		pKF->UpdateConnections();

		// Reconstruye las observaciones de todos los puntos
		size_t largo = pKF->mvpMapPoints.size();
		for(size_t i=0; i<largo; i++){
			MapPoint *pMP = pKF->mvpMapPoints[i];
			if (pMP)
				pMP->AddObservation(pKF, i);
		}


	}

	/*
	 * Recorre los MapPoints del mapa:
	 * - Determina el id máximo, para luego establecer MapPoint::nNextId
	 * - Reconstruye mpRefKF
	 * - Reconstruye propiedades con UpdateNormalAndDepth (usa mpRefKF)
	 */
	long unsigned int maxId = 0;
	for(MapPoint *pMP : mapa->mspMapPoints){
		// Busca el máximo id, para al final determinar nNextId
		maxId = max(maxId, pMP->mnId);

		// Reconstruye mpRefKF a partir de mnFirstKFid.  setRefKF escribe la propiedad protegida.  Requiere mObservations
		long unsigned int id = pMP->mnFirstKFid;
		std::set<KeyFrame*>::iterator it = std::find_if(mapa->mspKeyFrames.begin(), mapa->mspKeyFrames.end(), [id](KeyFrame *KF){return KF->mnId == id;});
		if(it != mapa->mspKeyFrames.end())
			pMP->mpRefKF = *it;
		else
			pMP->mpRefKF = (*pMP->mObservations.begin()).first;



		/* Reconstruye:
		 * - mNormalVector
		 * - mfMinDistance
		 * - mfMaxDistance
		 *
		 * mediante UpdateNormalAndDepth, que requiere haber reconstruído antes mpRefKF.
		 */
		pMP->UpdateNormalAndDepth();
	}
	MapPoint::nNextId = maxId + 1;

}

/**
 * Depura los conjuntos de keyframes y mappoints que constituyen el mapa->
 *
 * Realiza una chequeo de todos los keyframes en MapPoints.mObservations y mappoints en KeyFrames.mvpMapPoints,
 * constatando que:
 *
 * - no sean malos (isBad)
 * - estén en el mapa (en Map::mspMapPoints y Map::mspKeyFrames)
 *
 * Si no cumplen estos requisitos, los elimina.
 *
 * Con esto se evita la serialización de elementos retirados del mapa, que por error siguen apuntados desde algún lugar.
 *
 */
void Serializer::depurar(){
	cout << "\nDepurando..." << endl;
	//Primero se deben eliminar los puntos del keyframe, luego los keyframes de los puntos.

	// Anula puntos malos en el vector KeyFrame::mvpMapPoints
	for(auto &pKF: mapa->mspKeyFrames){	// Keyframes del mapa, en Map::mspKeyFrames
		auto pMPs = pKF->GetMapPointMatches();	// Vector KeyFrame::mvpMapPoints
		for(auto pMP: pMPs){	// Puntos macheados en el keyframe.
			bool borrar = false;

			if(!pMP) continue;	// Saltear si es NULL
			if(pMP->isBad()){	// Si el mappoint es malo...
				cout << "Mal MP:" << pMP->mnId << ". ";
				borrar = true;
			}
			if(!mapa->mspMapPoints.count(pMP)){
				cout << "MP no en mapa:" << pMP->mnId << ". ";
				//borrar = true;
			}
			if(borrar){
				// Para borrar el punto del vector en el keyframe, el keyframe debe poder encontrarse en las observaciones del punto.
				//pKF->EraseMapPointMatch(pMP);	// lo borra (lo pone NULL en el vector mvpMapPoints)

				// Buscar el punto en el vector
				int idx=pMPs.size();
				for(; --idx>=0;){
					auto pMP2 = pMPs[idx];
					if(pMP2 && pMP2 == pMP){
						// encontrado, borrar
						pKF->EraseMapPointMatch(idx);
						//break;// En vez de parar cuando lo enuentra, continúa por si aparece varias veces.
					}
				}
				pMPs = pKF->GetMapPointMatches();
				if(std::find(pMPs.begin(), pMPs.end(), pMP) != pMPs.end()){
					cout << "¡No se borró! ";
				}

				cout << "KF " << pKF->mnId << endl;
			}
		}
	}

	// Elimina keyframes malos en mObservations
	for(auto &pMP: mapa->mspMapPoints){	// Set de puntos del mapa
		auto obs = pMP->GetObservations();	// map, sin null, devuelve el mapa mObservations, mapa de keyframes que observan al punto
		for(auto it = obs.begin(); it != obs.end();){	// Keyframes que observan el punto.  Bucle para borrar selectivamente.
			KeyFrame* pKF = it->first;
			it++;
			bool borrar = false;

			if(pKF->isBad()){	// it->first es el keyframe, y si es malo...
				cout << "Mal KF:" << pKF->mnId << ". ";
				borrar = true;
			}
			if(!mapa->mspKeyFrames.count(pKF)){
				cout << "KF no en mapa:" << pKF->mnId << ". ";
				//borrar = true;
			}
			if(pKF->mbNotErase){
				cout << "Not Erase:" << pKF->mnId << ". ";
				borrar = false;

			}
			if(borrar){
				pMP->EraseObservation(pKF);	// se borra del mapa de observaciones
				if(pMP->GetIndexInKeyFrame(pKF)>=0) cout << "¡No se borró! ";
				cout << "MP " << pMP->mnId << endl;
			}
		}
	}
}

}	// namespace ORB_SLAM2
