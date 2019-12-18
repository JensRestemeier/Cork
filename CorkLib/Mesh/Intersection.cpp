// +-------------------------------------------------------------------------
// | mesh.isct.th
// | 
// | Author: Gilbert Bernstein
// +-------------------------------------------------------------------------
// | COPYRIGHT:
// |    Copyright Gilbert Bernstein 2013
// |    See the included COPYRIGHT file for further details.
// |    
// |    This file is part of the Cork library.
// |
// |    Cork is free software: you can redistribute it and/or modify
// |    it under the terms of the GNU Lesser General Public License as
// |    published by the Free Software Foundation, either version 3 of
// |    the License, or (at your option) any later version.
// |
// |    Cork is distributed in the hope that it will be useful,
// |    but WITHOUT ANY WARRANTY; without even the implied warranty of
// |    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// |    GNU Lesser General Public License for more details.
// |
// |    You should have received a copy 
// |    of the GNU Lesser General Public License
// |    along with Cork.  If not, see <http://www.gnu.org/licenses/>.
// +-------------------------------------------------------------------------


//	The gmpext4.h include has to happen first, otherwise we run into multiple declaration issues.

#include <random>

#include "..\Intersection\gmpext4.h"

#include <boost\noncopyable.hpp>
#include <boost\optional.hpp>

#include "tbb/tbb.h"

#include "..\Util\ManagedIntrusiveList.h"
#include "..\Util\ThreadingHelpers.h"

#include "..\Intersection\quantization.h"
#include "..\Intersection\empty3d.h"

#include "MeshBase.h"
#include "TopoCache.h"

#include "..\Accel\aabvh.h"

#include "..\Mesh\IntersectionProblem.h"

#include "..\Util\CachingFactory.h"




#define REAL double			//	This define is for triangle.h below

extern "C"
{
	#include "..\Intersection\triangle.h"
}



namespace Cork
{
	namespace Intersection
	{

		class PerturbationEpsilon
		{
		public:

			explicit
				PerturbationEpsilon( const Quantization::Quantizer&		quantizer )
				: m_bitsOfPurturbationRange( quantizer.bitsOfPurturbationRange() ),
				  m_quantum( quantizer.purturbationQuantum() ),
				  m_numAdjustments( 0 )
			{}

			PerturbationEpsilon() = delete;
			PerturbationEpsilon( const PerturbationEpsilon& ) = delete;


			bool							sufficientRange() const
			{
				return( ( m_bitsOfPurturbationRange - m_numAdjustments ) >= PERTURBATION_BUFFER_BITS + MINIMUM_PERTURBATION_RANGE_BITS );
			}

			NUMERIC_PRECISION				quantum() const
			{
				return( m_quantum );
			}

			int								numAdjustments() const
			{
				return( m_numAdjustments );
			}

			AdjustPerturbationResult		adjust()
			{
				m_numAdjustments++;

				m_randomRange <<= 1;

				if( !sufficientRange() )
				{
					return( AdjustPerturbationResult::Failure( AdjustPerturbationResultCodes::MAXIMUM_PERTURBATION_REACHED, "Maximum Perturbation reached" ) );
				}

				return( AdjustPerturbationResult( m_numAdjustments ) );
			}


			Cork::Math::Vector3D			getPerturbation() const
			{
				return( m_randMatrix.getPerturbation( m_numAdjustments, m_quantum ) );
			}


		private:

			int							m_bitsOfPurturbationRange;
			double						m_quantum;

			int							m_numAdjustments;

			int							m_randomRange;


			class PerturbationRandomizationMatrix
			{
			public:

				PerturbationRandomizationMatrix()
					: m_mersenneTwister( time( 0 ) )
				{
					for( int numPermutations = 4; numPermutations <= 32; numPermutations <<= 1 )
					{
						m_randomizationMatrix.push_back( std::vector<std::tuple<long, long, long>>() );

						auto&	currentVec = m_randomizationMatrix.back();

						for( int i = 0; i <= numPermutations; i++ )
						{
							for( int j = 0; j <= numPermutations; j++ )
							{
								for( int k = 0; k <= numPermutations; k++ )
								{
									currentVec.push_back( std::tuple<long, long, long>( i, j, k ) );

									if( i > 0 )
									{
										currentVec.push_back( std::tuple<long, long, long>( -i, j, k ) );
									}

									if( j > 0 )
									{
										currentVec.push_back( std::tuple<long, long, long>( i, -j, k ) );
									}

									if( k > 0 )
									{
										currentVec.push_back( std::tuple<long, long, long>( i, j, -k ) );
									}

									if(( i > 0 ) && ( j > 0 ))
									{
										currentVec.push_back( std::tuple<long, long, long>( -i, -j, k ) );
									}

									if( ( i > 0 ) && ( k > 0 ) )
									{
										currentVec.push_back( std::tuple<long, long, long>( -i, j, -k ) );
									}

									if( ( j > 0 ) && ( k > 0 ) )
									{
										currentVec.push_back( std::tuple<long, long, long>( i, -j, -k ) );
									}

									if( ( i > 0 ) && ( j > 0 ) && ( k > 0 ) )
									{
										currentVec.push_back( std::tuple<long, long, long>( -i, -j, -k ) );
									}
								}
							}
						}

						m_numEntries.push_back( currentVec.size() );
					}
				}


				Cork::Math::Vector3D			getPerturbation( int			index,
																 double			quantum ) 
				{
					if( index >= m_numEntries.size() )
					{
						return( getBruteForcePerturbation( index, quantum ) );
					}
					
					//	Using the standard clib random number generator std::rand() resulted in spurious errors
					//		in triangle consolidation.  This seems not to be the case with the MT generator.
					//
					//	I don't quite understand why this approach to creating an array of purturbation combinations
					//		and then choosing the right combo with a single random number is more fragile than 
					//		the prior method of generating random values for each offset but with std::rand() there
					//		were certainly problems.
					
					long		arrayIndex = m_mersenneTwister() % m_numEntries[index];

					const std::tuple<long, long, long>&		randEntry = m_randomizationMatrix[index][arrayIndex];

					return( Cork::Math::Vector3D( std::get<0>( randEntry ) * quantum, std::get<1>( randEntry ) * quantum, std::get<2>( randEntry ) * quantum ) );
				}


			private:

				std::vector<size_t>												m_numEntries;

				std::vector<std::vector<std::tuple<long, long, long>>>			m_randomizationMatrix;

				std::mt19937													m_mersenneTwister;



				Cork::Math::Vector3D			getBruteForcePerturbation( int			index,
																		   double		quantum )
				{
					//	We have overrun the size of the randomization table so compute the perturbation
					//		brute force with lots of random calls.

					int							perturbRange = 1 << ( index + 2 );

					Cork::Math::Vector3D		perturbation;

					perturbation = Cork::Math::Vector3D(( m_mersenneTwister() % perturbRange) *quantum, ( m_mersenneTwister() % perturbRange) *quantum, ( m_mersenneTwister() % perturbRange) *quantum);

					if(( m_mersenneTwister() % 2 ) == 1 )
					{
						perturbation[0] = -perturbation[0];
					}

					if( ( m_mersenneTwister() % 2 ) == 1 )
					{
						perturbation[1] = -perturbation[1];
					}

					if( ( m_mersenneTwister() % 2 ) == 1 )
					{
						perturbation[2] = -perturbation[2];
					}

					return( perturbation );
				}

			};


			static PerturbationRandomizationMatrix		m_randMatrix;
		};

		//	Define the static global so it is initialized

		PerturbationEpsilon::PerturbationRandomizationMatrix		PerturbationEpsilon::m_randMatrix;



		class TriTripleTemp
		{
		public :

			TriTripleTemp( const TopoTri&		tp0,
						   const TopoTri&		tp1,
						   const TopoTri&		tp2 )
				: t0( tp0 ),
				  t1( tp1 ),
				  t2( tp2 )
			{}


			const TopoTri& t0;
			const TopoTri& t1;
			const TopoTri& t2;
		};




		inline
		Cork::Math::Vector3D		computeCoords( const TopoEdge&				e,
												   const TopoTri&				t,
												   const Quantization::Quantizer& quantizer)
		{
			GMPExt4::GmpExt4_2		edgeCoordinates( e.edgeExactCoordinates( quantizer ) );
			GMPExt4::GmpExt4_3		triangleCoordinates( t.triangleExactCoordinates( quantizer ) );

			return( Empty3d::coordsExact( edgeCoordinates, triangleCoordinates, quantizer ) );
		}


		inline
		Cork::Math::Vector3D		computeCoords( const TopoTri&						t0,
												   const TopoTri&						t1,
												   const TopoTri&						t2,
												   const Quantization::Quantizer&		quantizer )
		{
			return( Empty3d::coordsExact( t0.triangleExactCoordinates( quantizer ), t1.triangleExactCoordinates( quantizer ), t2.triangleExactCoordinates( quantizer ), quantizer ) );
		}





		//	A handful of forward declarations.  The following classes are somewhat intertwined and though I could pry them apart, it would end up moving
		//		code out of the class declarations which would make things a bit less transparent.

		class GenericVertType;
		typedef GenericVertType IsctVertType;

		class GenericEdgeType;




		class GluePointMarker : public boost::noncopyable, public IntrusiveListHook
		{
		public:

			typedef std::vector<IsctVertType*>		IntersectionVertexCollection;


			GluePointMarker( bool				splitType,
							 bool				edgeTriType,
							 const TopoEdge&	eisct,
							 const TopoTri&		tisct )
				: m_splitType( splitType ),
				  m_edgeTriType( edgeTriType ),
				  m_e( eisct ),
				  m_t({ { &tisct, nullptr, nullptr} })
			{
				m_copies.reserve( 16 );
			}

			GluePointMarker( bool				splitType,
							 bool				edgeTriType,
							 const TopoTri&		tisct0,
							 const TopoTri&		tisct1,
							 const TopoTri&		tisct2 )
				: m_splitType( splitType ),
				  m_edgeTriType( edgeTriType ),
				  m_t({ { &tisct0, &tisct1, &tisct2 } })
			{
				m_copies.reserve( 16 );			
			}


			~GluePointMarker()
			{}


			bool									splitType() const
			{
				return( m_splitType );
			}

			bool									edgeTriType() const
			{
				return( m_edgeTriType );
			}

			boost::optional<const TopoEdge&>&		edge()
			{
				return( m_e );
			}

			std::array<const TopoTri*, 3>&			triangles()
			{
				return( m_t );
			}

			const IntersectionVertexCollection&		copies() const
			{
				return( m_copies );
			}

			void									addVertexToCopies( IsctVertType*			vertex )
			{
				m_copies.push_back(vertex);
			}

			void									removeVertexFromCopies( IsctVertType*		vertex )
			{
				m_copies.erase(std::find(m_copies.begin(), m_copies.end(), vertex));
			}


		private:

			// list of all the vertices to be glued...

			IntersectionVertexCollection		m_copies;

			bool								m_splitType;		// splits are introduced manually, not by intersection and therefore use only a pointer
			bool								m_edgeTriType;		// true if edge-tri intersection - false if tri-tri-tri
	
			boost::optional<const TopoEdge&>	m_e;
	
			std::array<const TopoTri*, 3>		m_t;
		};


		typedef ManagedIntrusiveValueList<GluePointMarker>		GluePointMarkerList;




		class GenericVertType : public boost::noncopyable, public IntrusiveListHook
		{
		public :

			enum class VertexType { INTERSECTION, ORIGINAL };

			typedef std::vector<GenericEdgeType*>		EdgeCollection;


			GenericVertType()
			{
				m_edges.reserve( 16 );
			}

			GenericVertType( VertexType							type,
							 TopoVert&							concreteVertex,
							 const Cork::Math::Vector3D&		coordinate )
				: m_vertexType( type ),
				  m_concreteVertex( concreteVertex ),
				  m_coordinate( coordinate )
			{
				m_edges.reserve( 16 );			
			}

			GenericVertType( VertexType							type,
							 TopoVert&							concreteVertex,
							 const Cork::Math::Vector3D&		coordinate,
							 bool								boundary )
				: m_vertexType( type ),
				  m_concreteVertex( concreteVertex ),
				  m_coordinate( coordinate ),
				  m_boundary( boundary )
			{
				m_edges.reserve( 16 );			
			}

			GenericVertType( VertexType							type,
							 TopoVert&							concreteVertex,
							 const Cork::Math::Vector3D&		coordinate,
							 bool								boundary,
							 GluePointMarker&					glueMarker )
				: m_vertexType( type ),
				  m_concreteVertex( concreteVertex ),
				  m_coordinate( coordinate ),
				  m_boundary( boundary ),
				  m_glueMarker( glueMarker )
			{
				m_edges.reserve( 16 );

				m_glueMarker->addVertexToCopies( this );
			}

			GenericVertType( VertexType							type,
							 const Cork::Math::Vector3D&		coordinate,
							 bool								boundary,
							 GluePointMarker&					glueMarker )
				: m_vertexType( type ),
				  m_coordinate( coordinate ),
				  m_boundary( boundary ),
				  m_glueMarker( glueMarker )
			{
				m_edges.reserve( 16 );

				m_glueMarker->addVertexToCopies( this );
			}


			~GenericVertType()
			{}



			const VertexType					vertexType() const
			{
				return( m_vertexType );
			}


			boost::optional<TopoVert&>&			concreteVertex()
			{
				return( m_concreteVertex );
			}

			void								setConcreteVertex( TopoVert&		concreteVertex )
			{
				m_concreteVertex.emplace( concreteVertex );
			}


			const Cork::Math::Vector3D&			coordinate() const
			{
				return( m_coordinate );
			}


			bool								isBoundary() const
			{
				return( m_boundary );
			}

			void								setBoundary( bool	newValue )
			{
				m_boundary = newValue;
			}

	
			const EdgeCollection&				edges() const
			{
				return( m_edges );
			}

			EdgeCollection&						edges()
			{
				return( m_edges );
			}

    
			uint								index() const
			{
				return( m_index );
			}

			void								setIndex( uint		newValue )
			{
				m_index = newValue;
			}
     

			const GluePointMarker&				glueMarker() const
			{
				return( m_glueMarker.value() );
			}

			GluePointMarker&					glueMarker()
			{
				return( m_glueMarker.value() );
			}

			void								removeFromGlueMarkerCopies()
			{
				m_glueMarker->removeVertexFromCopies(this);
			}


		private :

			VertexType								m_vertexType;
			boost::optional<TopoVert&>				m_concreteVertex;
			bool									m_boundary;

			uint									m_index;				// temporary for triangulation marshalling

			Cork::Math::Vector3D					m_coordinate;

			EdgeCollection							m_edges;

			boost::optional<GluePointMarker&>		m_glueMarker;
		};


		typedef GenericVertType			IsctVertType;
		typedef GenericVertType			OrigVertType;

		typedef ManagedIntrusiveValueList<IsctVertType>			IsctVertTypeList;
		typedef ManagedIntrusivePointerList<IsctVertType>		IntersectionVertexPointerList;

		typedef ManagedIntrusiveValueList<OrigVertType>			OrigVertTypeList;




		class GenericEdgeType : public boost::noncopyable, public IntrusiveListHook
		{
		public:

			enum class EdgeType { INTERSECTION, ORIGINAL, SPLIT };

			typedef std::vector<IsctVertType*>		IntersectionVertexList;


			GenericEdgeType( EdgeType			type,
							 bool				boundary,
							 GenericVertType*	endpoint )
				: m_type( type ),
				  m_boundary( boundary )
			{
				m_interior.reserve( 16 );
				
				m_ends[0] = endpoint;
				endpoint->edges().push_back( this );

				m_ends[1] = nullptr;
			}


			GenericEdgeType( EdgeType			type,
							 const TopoEdge&	concrete,
							 bool				boundary,
							 GenericVertType*	endpoint )
				: m_type( type ),
				  m_concrete(concrete),
				  m_boundary(boundary)
			{
				m_interior.reserve( 16 );
				
				m_ends[0] = endpoint;
				endpoint->edges().push_back(this);

				m_ends[1] = nullptr;
			}

			GenericEdgeType( EdgeType					type,
							 bool						boundary,
							 GenericVertType*			endpoint1,
							 GenericVertType*			endpoint2 )
				: m_type( type ),
				  m_boundary( boundary )
			{
				m_interior.reserve( 16 );
				
				m_ends[0] = endpoint1;
				endpoint1->edges().push_back( this );

				m_ends[1] = endpoint2;
				endpoint2->edges().push_back( this );;
			}

			GenericEdgeType( EdgeType					type,
							 const TopoEdge&			concrete,
							 bool						boundary,
							 GenericVertType*			endpoint1,
							 GenericVertType*			endpoint2 )
							 : m_type( type ),
							 m_concrete( concrete ),
							 m_boundary( boundary )
			{
				m_interior.reserve( 16 );
				
				m_ends[0] = endpoint1;
				endpoint1->edges().push_back( this );

				m_ends[1] = endpoint2;
				endpoint2->edges().push_back( this );;
			}

			GenericEdgeType( EdgeType					type,
							 const TopoEdge&			concrete,
							 bool						boundary,
							 GenericVertType*			endpoint,
							 const TopoTri&				otherTriKey )
				: m_type( type ),
				  m_concrete( concrete ),
				  m_boundary( boundary ),
				  m_otherTriKey( otherTriKey )
			{
				m_interior.reserve( 16 );
				
				m_ends[0] = endpoint;
				endpoint->edges().push_back( this );

				m_ends[1] = nullptr;
			}

			GenericEdgeType( EdgeType					type,
							 bool						boundary,
							 GenericVertType*			endpoint,
							 const TopoTri&				otherTriKey )
							 : m_type( type ),
							 m_boundary( boundary ),
							 m_otherTriKey( otherTriKey )
			{
				m_interior.reserve( 16 );
				
				m_ends[0] = endpoint;
				endpoint->edges().push_back( this );

				m_ends[1] = nullptr;
			}





			EdgeType									edgeType() const
			{
				return( m_type );
			}

			const boost::optional<const TopoEdge&>&		concrete() const
			{
				return( m_concrete );
			}

	
			bool										boundary() const
			{
				return( m_boundary );
			}


			const std::array<GenericVertType*, 2>&		ends() const
			{
				return( m_ends );
			}

			std::array<GenericVertType*, 2>&			ends()
			{
				return( m_ends );
			}


			const IntersectionVertexList&				interior() const
			{
				return( m_interior );
			}

			IntersectionVertexList&						interior()
			{
				return( m_interior );
			}

			const boost::optional<const TopoTri&>&		otherTriKey() const
			{
				return( m_otherTriKey );
			}


			void		disconnect()
			{
				m_ends[0]->edges().erase( std::find( m_ends[0]->edges().begin(), m_ends[0]->edges().end(), this ) );
				m_ends[1]->edges().erase( std::find( m_ends[1]->edges().begin(), m_ends[1]->edges().end(), this ) );

				for (IsctVertType* iv : m_interior )
				{
					iv->edges().erase( std::find( iv->edges().begin(), iv->edges().end(), this ) );
				}
			}


		private :

			EdgeType							m_type;

			boost::optional<const TopoEdge&>	m_concrete;
    
			bool								m_boundary;

			std::array<GenericVertType*, 2>		m_ends;
    
			IntersectionVertexList				m_interior;


			boost::optional<const TopoTri&>		m_otherTriKey;		 // use to detect duplicate instances within a triangle
		};


		typedef GenericEdgeType		IsctEdgeType;
		typedef GenericEdgeType		OrigEdgeType;
		typedef GenericEdgeType		SplitEdgeType;

		typedef ManagedIntrusiveValueList<IsctEdgeType>			IsctEdgeTypeList;
		typedef ManagedIntrusiveValueList<OrigEdgeType>			OrigEdgeTypeList;
		typedef ManagedIntrusiveValueList<SplitEdgeType>		SplitEdgeTypeList;

		typedef ManagedIntrusivePointerList<IsctEdgeType>		IntersectionEdgePointerList;




		class GenericTriType : public boost::noncopyable, public IntrusiveListHook
		{
		public :

			GenericTriType( GenericVertType* v0, GenericVertType* v1, GenericVertType* v2 )
				: m_concreteTriangle( nullptr )
			{
				m_vertices[0] = v0;
				m_vertices[1] = v1;
				m_vertices[2] = v2;
			}


	
			const TopoTri*		concreteTriangle() const
			{
				return( m_concreteTriangle );
			}
	
			void				setConcreteTriangle( TopoTri*		concreteTriangle )
			{
				m_concreteTriangle = concreteTriangle;
			}


			const std::array<GenericVertType*, 3>		vertices() const
			{
				return( m_vertices );
			}



		private :

			TopoTri *							m_concreteTriangle;
    
			std::array<GenericVertType*,3>		m_vertices;
		};


		typedef ManagedIntrusiveValueList<GenericTriType>		GenericTriTypeList;
		typedef ManagedIntrusivePointerList<GenericTriType>		GenericTriPointerList;



		//	Forward declare the context

		class IntersectionProblemWorkspaceBase : public TopoCacheWorkspace
		{
		public:

			virtual ~IntersectionProblemWorkspaceBase()
			{}

			virtual void	reset() = 0;

			virtual operator GluePointMarkerList::PoolType&() = 0;
			virtual operator IsctVertTypeList::PoolType&() = 0;
			virtual operator IsctEdgeTypeList::PoolType&() = 0;
			virtual operator GenericTriTypeList::PoolType&() = 0;
			virtual operator IntersectionVertexPointerList::PoolType&() = 0;
			virtual operator IntersectionEdgePointerList::PoolType&() = 0;
			virtual operator GenericTriPointerList::PoolType&() = 0;

			virtual operator Cork::AABVH::Workspace&() = 0;
		};


		class IntersectionProblemBase  : public TopoCache
		{
		public :

			class TriAndEdgeQueueMessage
			{
			public :

				enum class MessageType { TRI_AND_INTERSECTING_EDGES, END_OF_MESSAGES };

				virtual MessageType			type() const = 0;
			};



			class TriAndEdgeQueueEnd : public TriAndEdgeQueueMessage
			{
			public:

				MessageType			type() const
				{
					return( MessageType::END_OF_MESSAGES );
				}
			};



			class TriangleAndIntersectingEdgesMessage : public TriAndEdgeQueueMessage
			{
			public :

				TriangleAndIntersectingEdgesMessage( TopoTri&					tri,
													 TopoEdgePointerVector&		edges )
					: m_triangle( tri ),
					  m_edges( edges )
				{}


				MessageType						type() const
				{
					return( MessageType::TRI_AND_INTERSECTING_EDGES );
				}

				TopoTri&						triangle()
				{
					return( m_triangle );
				}

				TopoEdgePointerVector&			edges()
				{
					return( m_edges );
				}
			
			private :

				TopoTri&						m_triangle;
				TopoEdgePointerVector			m_edges;
			};


			typedef tbb::concurrent_bounded_queue<TriAndEdgeQueueMessage*>		TriangleAndIntersectingEdgesQueue;



			IntersectionProblemBase( MeshBase&									owner,
									 const Quantization::Quantizer&				quantizer,
									 const Cork::Math::BBox3D&					intersectionBBox,
									 IntersectionProblemWorkspaceBase&			workspace );

			IntersectionProblemBase( const IntersectionProblemBase&			isctProblemToCopy ) = delete;

			IntersectionProblemBase&			operator=( const IntersectionProblemBase& ) = delete;

			virtual ~IntersectionProblemBase()
			{}



			IntersectionProblemWorkspaceBase&			getWorkspace()
			{
				return(m_workspace);
			}

			Empty3d::ExactArithmeticContext&			ExactArithmeticContext()
			{
				return( m_exactArithmeticContext );
			}

			IsctVertType* newIsctVert(const TopoEdge& e, const TopoTri& t, bool boundary, GluePointMarker& glue)
			{
				return( m_isctVertTypeList.emplace_back( GenericVertType::VertexType::INTERSECTION, computeCoords( e, t, m_quantizer ), boundary, glue ) );
			}


			IsctVertType* newIsctVert( const TopoTri& t0, const TopoTri& t1, const TopoTri& t2, bool boundary, GluePointMarker& glue)
			{
				return( m_isctVertTypeList.emplace_back( GenericVertType::VertexType::INTERSECTION, computeCoords( t0, t1, t2, m_quantizer ), boundary, glue ) );
			}

			IsctVertType* newSplitIsctVert(const Cork::Math::Vector3D& coords, GluePointMarker& glue)
			{
				return(m_isctVertTypeList.emplace_back(GenericVertType::VertexType::INTERSECTION, coords, false, glue));
			}

			IsctVertType* copyIsctVert(IsctVertType* orig)
			{
				return(m_isctVertTypeList.emplace_back(GenericVertType::VertexType::INTERSECTION, orig->coordinate(), orig->isBoundary(), orig->glueMarker()));
			}

			IsctEdgeType* newIsctEdge(IsctVertType* endpoint, const TopoTri& tri_key)
			{
				return(m_isctEdgeTypeList.emplace_back(GenericEdgeType::EdgeType::INTERSECTION, false, endpoint, tri_key));
			}

			OrigVertType* newOrigVert( TopoVert*		v)
			{
				return(m_origVertTypeList.emplace_back(GenericVertType::VertexType::ORIGINAL, *v, *(v->quantizedValue()), true));
			}

			OrigEdgeType* newOrigEdge(const TopoEdge& e, OrigVertType* v0, OrigVertType* v1)
			{
				return(m_origEdgeTypeList.emplace_back(GenericEdgeType::EdgeType::ORIGINAL, e, true, v0, v1));
			}

			SplitEdgeType* newSplitEdge(GenericVertType* v0, GenericVertType* v1, bool boundary)
			{
				return(m_splitEdgeTypeList.emplace_back(GenericEdgeType::EdgeType::SPLIT, boundary, v0, v1));
			}

			GenericTriType* newGenericTri(GenericVertType* v0, GenericVertType* v1, GenericVertType* v2)
			{
				return(m_genericTriTypeList.emplace_back(v0, v1, v2));
			}




			void perturbPositions()
			{
				for( int i = 0; i < m_quantizedCoords.size(); i++ )
				{
					Cork::Math::Vector3D	perturbation = m_perturbation.getPerturbation();

					m_quantizedCoords[i] = m_quantizedCoords[i] + perturbation;
				}
			}



			void					CreateBoundingVolumeHierarchy()
			{
				std::unique_ptr< Cork::AABVH::GeomBlobVector >		edge_geoms( new Cork::AABVH::GeomBlobVector() );

				edge_geoms->reserve( edges().size() );
				
				for( auto& e : edges() )
				{
					edge_geoms->emplace_back( e );
				}

				m_edgeBVH.reset( new Cork::AABVH::AxisAlignedBoundingVolumeHierarchy( edge_geoms, m_workspace, ownerMesh().solverControlBlock() ) );
			}



			void			FindEdgeAndTriangleIntersections( Cork::AABVH::IntersectionType				selfOrBooleanIntersection,
															  TriangleAndIntersectingEdgesQueue&		triangleAndEdges )
			{
				CreateBoundingVolumeHierarchy();

				//	Search for intersections, either in multiple threads or in a single thread 
/*
				if( ownerMesh().solverControlBlock().useMultipleThreads() )
				{
					//	Multithreaded search

					assert( triangles().isCompact() );

					tbb::parallel_for( tbb::blocked_range<TopoTriList::PoolType::iterator>( triangles().getPool().begin(), triangles().getPool().end(), ( triangles().getPool().size() / 4 ) - 1 ),
						[&] ( tbb::blocked_range<TopoTriList::PoolType::iterator> triangles )
					{
						TopoEdgePointerVector			edges;

						for( TopoTri& t : triangles )
						{
							m_edgeBVH->EdgesIntersectingTriangle( t, selfOrBooleanIntersection, edges );

							if (!edges.empty())
							{
								triangleAndEdges.push( new TriangleAndIntersectingEdgesMessage( t, edges ));

								edges.clear();
							}
						}
					}, tbb::simple_partitioner() );
				}
				else
				{
				*/
					//	Single threaded search

					TopoEdgePointerVector			edges;

					for( TopoTri& tri : triangles() )
					{
						m_edgeBVH->EdgesIntersectingTriangle( tri, selfOrBooleanIntersection, edges );

						if (!edges.empty())
						{
							triangleAndEdges.push( new TriangleAndIntersectingEdgesMessage( tri, edges ) );

							edges.clear();
						}
					}
//				}

				//	Push the end of queue message

				triangleAndEdges.push( new TriAndEdgeQueueEnd() );
			}


			void createRealPtFromGluePt( GluePointMarker& glue )
			{
				ENSURE( glue.copies().size() > 0 );
				
				TopoVert*			v = TopoCache::newVert();

				TopoCache::ownerMesh().vertices()[v->ref()] = glue.copies()[0]->coordinate();

				for (IsctVertType* iv : glue.copies())
				{
					iv->setConcreteVertex( *v );
				}
			}



			void releaseEdge(GenericEdgeType* ge)
			{
				ge->disconnect();

				IsctEdgeType*       ie = dynamic_cast<IsctEdgeType*>(ge);

				switch (ge->edgeType())
				{
				case GenericEdgeType::EdgeType::INTERSECTION:
					m_isctEdgeTypeList.free(ie);
					break;

				case GenericEdgeType::EdgeType::ORIGINAL:
					m_origEdgeTypeList.free(ie);
					break;

				case GenericEdgeType::EdgeType::SPLIT:
					m_splitEdgeTypeList.free(ie);
					break;
				}
			}


			void	killIsctVert( IsctVertType* iv )
			{
				iv->removeFromGlueMarkerCopies();

				if (iv->glueMarker().copies().size() == 0)
				{
					m_gluePointMarkerList.free( iv->glueMarker() );
				}

				for (GenericEdgeType* ge : iv->edges())
				{
					// disconnect
					ge->interior().erase( std::find( ge->interior().begin(), ge->interior().end(), iv ) );

					if (ge->ends()[0] == iv)
					{
						ge->ends()[0] = nullptr;
					}

					if (ge->ends()[1] == iv)
					{
						ge->ends()[1] = nullptr;
					}
				}

				m_isctVertTypeList.free( iv );
			}



			void killIsctEdge( IsctEdgeType* ie )
			{
				// an endpoint may be an original vertex

				if (ie->ends()[1])
				{
					ie->ends()[1]->edges().erase( std::find( ie->ends()[1]->edges().begin(), ie->ends()[1]->edges().end(), ie ) );
				}

				m_isctEdgeTypeList.free( ie );
			}


			void killOrigVert( OrigVertType* ov )
			{
				m_origVertTypeList.free( ov );
			}


			void killOrigEdge( OrigEdgeType* oe )
			{
				m_origEdgeTypeList.free( oe );
			}



			bool checkIsct( const TopoTri& t0, const TopoTri& t1, const TopoTri& t2 )
			{
				// This function should only be called if we've already
				// identified that the intersection edges
				//      (t0,t1), (t0,t2), (t1,t2)
				// exist.
				//
				// From this, we can conclude that each pair of triangles
				// shares no more than a single vertex in common.
				//
				// If each of these shared vertices is different from each other,
				// then we could legitimately have a triple intersection point,
				// but if all three pairs share the same vertex in common, then
				// the intersection of the three triangles must be that vertex.
				// So, we must check for such a single vertex in common amongst
				// the three triangles

				TopoVert*		common;

				if (t0.findCommonVertex( t1, common ))
				{
					for (uint i = 0; i<3; i++)
					{
						if (common == t2.verts()[i])
						{
							return( false );
						}
					}
				}

				//	Visual Studio's IDE gripes about the following when the explicit operator call is omitted

				Empty3d::TriTriTriIn input( t0.operator Empty3d::TriIn(), t1.operator Empty3d::TriIn(), t2.operator Empty3d::TriIn() );

				return( !input.emptyExact( m_quantizer, m_exactArithmeticContext ) );
			}


			void fillOutTriData( const TopoTri&		piece,
								 const TopoTri&		parent )
			{
				TopoCache::ownerMesh().triangles()[piece.ref()].boolAlgData() = ownerMesh().triangles()[parent.ref()].boolAlgData();
				TopoCache::ownerMesh().triangles()[piece.ref()].id() = ownerMesh().triangles()[parent.ref()].id();
			}



			void dumpIsctPoints( std::vector<Cork::Math::Vector3D>*		points )
			{
				points->resize( m_gluePointMarkerList.size() );

				uint write = 0;

				for (auto& glue : m_gluePointMarkerList)
				{
					ENSURE( glue.copies().size() > 0 );
					IsctVertType*       iv = glue.copies()[0];

					( *points )[write] = iv->coordinate();
					write++;
				}
			}



		protected:

			typedef Cork::Math::Vertex3DVector				QuantizedCoordinatesVector;

			IntersectionProblemWorkspaceBase&				m_workspace;

			std::unique_ptr<Cork::AABVH::AxisAlignedBoundingVolumeHierarchy>		m_edgeBVH;

			Empty3d::ExactArithmeticContext					m_exactArithmeticContext;

			aligned_unique_ptr<Cork::Math::BBox3D>			m_intersectionBBox;

			//	Quantizer must be in front of the Perturbation as the perturbation initialization depends on the quantizer

			Quantization::Quantizer							m_quantizer;
			PerturbationEpsilon								m_perturbation;

			QuantizedCoordinatesVector						m_quantizedCoords;
			
			GluePointMarkerList								m_gluePointMarkerList;
			IsctVertTypeList								m_isctVertTypeList;
			OrigVertTypeList								m_origVertTypeList;
			IsctEdgeTypeList								m_isctEdgeTypeList;
			OrigEdgeTypeList								m_origEdgeTypeList;
			SplitEdgeTypeList								m_splitEdgeTypeList;
			GenericTriTypeList								m_genericTriTypeList;
		};





		class EdgeCache
		{
		public:

			explicit
			EdgeCache( IntersectionProblemBase&		intersectionProblem )
				: m_intersectionProblem( intersectionProblem ),
				  m_edges( intersectionProblem.ownerMesh().vertices().size() )
			{}

			TopoEdge* operator()( TopoVert& v0, TopoVert& v1 )
			{
				IndexType	i = v0.ref();
				IndexType	j = v1.ref();

				if (i > j)
				{
					std::swap( i, j );
				}

				size_t		N = m_edges[i].size();

				for( size_t k = 0; k < N; k++ )
				{
					if (m_edges[i][k].vid == j)
					{
						return( m_edges[i][k].e );
					}
				}

				// if not existing, create it

				m_edges[i].emplace_back( EdgeEntry( j ) );

				TopoEdge* e = m_edges[i][N].e = m_intersectionProblem.newEdge();

				e->verts()[0] = &v0;
				e->verts()[1] = &v1;

				v0.edges().insert( e );
				v1.edges().insert( e );

				return( e );
			}

			// k = 0, 1, or 2
			TopoEdge* getTriangleEdge( GenericTriType* gt, uint k, const TopoTri& big_tri )
			{
				GenericVertType*   gv0 = gt->vertices()[( k + 1 ) % 3];
				GenericVertType*   gv1 = gt->vertices()[( k + 2 ) % 3];
				TopoVert&    v0 = gv0->concreteVertex().value();
				TopoVert&    v1 = gv1->concreteVertex().value();

				// if neither of these are intersection points,
				// then this is a pre-existing edge...

				TopoEdge*    e = nullptr;

				if (( gv0->vertexType() == GenericVertType::VertexType::ORIGINAL ) &&
					( gv1->vertexType() == GenericVertType::VertexType::ORIGINAL ))
				{
					// search through edges of original triangle...
					for (uint c = 0; c<3; c++)
					{
						TopoVert* corner0 = big_tri.verts()[( c + 1 ) % 3];
						TopoVert* corner1 = big_tri.verts()[( c + 2 ) % 3];

						if ((( corner0 == &v0 ) && ( corner1 == &v1 )) ||
							(( corner0 == &v1 ) && ( corner1 == &v0 )))
						{
							e = big_tri.edges()[c];
						}
					}

					ENSURE( e ); // Yell if we didn't find an edge
				}
				// otherwise, we need to check the cache to find this edge
				else
				{
					e = operator()( v0, v1 );
				}
				return e;
			}

			TopoEdge* maybeEdge( GenericEdgeType* ge )
			{
				IndexType	i = ge->ends()[0]->concreteVertex()->ref();
				IndexType	j = ge->ends()[1]->concreteVertex()->ref();

				if (i > j)
				{
					std::swap( i, j );
				}

				size_t		N = m_edges[i].size();

				for (size_t k = 0; k<N; k++)
				{
					if (m_edges[i][k].vid == j)
					{
						return( m_edges[i][k].e );
					}
				}

				// if we can't find it
				return( nullptr );
			}



		private:

			struct EdgeEntry
			{
				EdgeEntry()
				{}

				explicit
				EdgeEntry( IndexType	id )
					: vid( id )
				{}


				IndexType		vid;
				TopoEdge*		e;
			};


			typedef	std::vector< std::vector<EdgeEntry> >		VectorOfEdgeEntryVectors;

			IntersectionProblemBase&		m_intersectionProblem;

			VectorOfEdgeEntryVectors		m_edges;
		};





		class TriangleProblem : public boost::noncopyable, public IntrusiveListHook
		{
		public:

			TriangleProblem( IntersectionProblemBase&		iprob,
							 const TopoTri&					triangle )
				: m_iprob( iprob ),
				  m_triangle( triangle ),
				  m_iverts(iprob.getWorkspace()),
				  m_iedges(iprob.getWorkspace()),
				  m_gtris(iprob.getWorkspace())
			{
				//	m_triangle can be const... just about everywhere.  This link is the only non-const operation,
				//		so let's explicitly cast away the const here but leave it everytwhere else.

				const_cast<TopoTri&>(m_triangle).setData(this);

				// extract original edges/verts

				for (uint k = 0; k < 3; k++)
				{
					overts[k] = m_iprob.newOrigVert(m_triangle.verts()[k]);
				}

				for (uint k = 0; k<3; k++)
				{
					oedges[k] = iprob.newOrigEdge( *(m_triangle.edges()[k]), overts[(k + 1) % 3], overts[(k + 2) % 3]);
				}
			}

			~TriangleProblem()
			{}


			//	Accessors and Mutators

			const TopoTri&								triangle() const
			{
				return(m_triangle);
			}

			void										ResetTopoTriLink()
			{
				const_cast<TopoTri&>(m_triangle).setData(nullptr);
			}

			const IntersectionEdgePointerList&			iedges() const
			{
				return(m_iedges);
			}

			IntersectionEdgePointerList&				iedges()
			{
				return(m_iedges);
			}

			const GenericTriPointerList&				gtris() const
			{
				return(m_gtris);
			}

			GenericTriPointerList&						gtris()
			{
				return(m_gtris);
			}


			// specify reference glue point and edge piercing this triangle.
			IsctVertType* addInteriorEndpoint( const TopoEdge&		edge,
											   GluePointMarker&		glue )
			{
				IsctVertType*       iv = m_iprob.newIsctVert(edge, m_triangle, false, glue);
				m_iverts.push_back(iv);

				for ( auto tri_key : edge.triangles())
				{
					addEdge(iv, *tri_key );
				}

				return(iv);
			}

			// specify the other triangle cutting this one, the edge cut,
			// and the resulting point of intersection

			void addBoundaryEndpoint( const TopoTri& tri_key, const TopoEdge& edge, IsctVertType* iv)
			{
				iv = m_iprob.copyIsctVert(iv);
				addBoundaryHelper(edge, iv);

				// handle edge extending into interior

				addEdge(iv, tri_key);
			}

			IsctVertType* addBoundaryEndpoint( const TopoTri& tri_key, const TopoEdge& edge, const Cork::Math::Vector3D& coord, GluePointMarker& glue)
			{
				IsctVertType*       iv = m_iprob.newSplitIsctVert(coord, glue);
				addBoundaryHelper(edge, iv);

				// handle edge extending into interior

				addEdge(iv, tri_key);

				return(iv);
			}

			// Should only happen for manually inserted split points on
			// edges, not for points computed via intersection...

			IsctVertType* addBoundaryPointAlone(const TopoEdge& edge, const Cork::Math::Vector3D& coord, GluePointMarker& glue)
			{
				IsctVertType*       iv = m_iprob.newSplitIsctVert(coord, glue);
				addBoundaryHelper(edge, iv);

				return(iv);
			}

			void addInteriorPoint(const TopoTri& t0, const TopoTri& t1, GluePointMarker& glue)
			{
				// note this generates wasted re-computation of coordinates 3X

				IsctVertType*       iv = m_iprob.newIsctVert( m_triangle, t0, t1, false, glue);
				m_iverts.push_back(iv);

				// find the 2 interior edges

				for (IsctEdgeType* ie : m_iedges)
				{
					if (( &(ie->otherTriKey().value()) == &t0) || ( &(ie->otherTriKey().value()) == &t1))
					{
						ie->interior().push_back(iv);
						iv->edges().push_back(ie);
					}
				}
			}

			// run after we've accumulated all the elements

			enum class ConsolidateResultCodes { SUCCESS = 0, COULD_NOT_FIND_COMMON_VERTEX };

			typedef SEFUtility::Result<ConsolidateResultCodes>		ConsolidateResult;


			ConsolidateResult	Consolidate()
			{
				// identify all intersection edges missing endpoints
				// and check to see if we can assign an original vertex
				// as the appropriate endpoint.

				for (IsctEdgeType* ie : m_iedges)
				{
					if (ie->ends()[1] == nullptr)
					{
						// try to figure out which vertex must be the endpoint...

						TopoVert* vert;

						if (!m_triangle.findCommonVertex( ie->otherTriKey().value(), vert))
						{
#ifdef _DEBUG

							std::cout << "the  edge is " << ie->ends()[0] << ",  " << ie->ends()[1] << std::endl;

							IsctVertType* iv = dynamic_cast<IsctVertType*>(ie->ends()[0]);

							std::cout << "   " << iv->glueMarker().edgeTriType() << std::endl;
							std::cout << "the   tri is " << m_triangle << ": " << m_triangle << std::endl;
							std::cout << "other tri is " << &(ie->otherTriKey().value()) << ": " << ie->otherTriKey().value() << std::endl;
							std::cout << "coordinates for triangles" << std::endl;
							std::cout << "the tri" << std::endl;

							for (uint k = 0; k<3; k++)
							{
								std::cout << *(m_triangle.verts()[k]->quantizedValue()) << std::endl;
							}

							for (uint k = 0; k<3; k++)
							{
								std::cout << *(ie->otherTriKey().value().verts()[k]->quantizedValue()) << std::endl;
							}

							std::cout << "degen count:" << m_iprob.ExactArithmeticContext().degeneracy_count << std::endl;
							std::cout << "exact count: " << m_iprob.ExactArithmeticContext().exact_count << std::endl;
#endif

							return( ConsolidateResult::Failure( ConsolidateResultCodes::COULD_NOT_FIND_COMMON_VERTEX, "Could not find common vertex in Triangle Problem Consolidate" ));
						}

						// then, find the corresponding OrigVertType*, and connect

						for (uint k = 0; k<3; k++)
						{
							if ( &(overts[k]->concreteVertex().value()) == vert)
							{
								ie->ends()[1] = overts[k];
								overts[k]->edges().push_back(ie);
								break;
							}
						}
					}
				}

				return( ConsolidateResult::Success() );
			}


			
			enum SubdivideResultCodes { SUCCESS = 0,
										SELF_INTERSECTING_MESH,
										TRIANGULATE_OUT_POINT_COUNT_UNEQUAL_TO_IN_POINT_COUNT };

			typedef SEFUtility::Result<SubdivideResultCodes>		SubdivideResult;

			SubdivideResult	Subdivide()
			{
				// collect all the points, and create more points as necessary

				std::vector<GenericVertType*>		points;
				points.reserve(1024);

				for (uint k = 0; k<3; k++)
				{
					points.push_back(overts[k]);
				}

				for (IsctVertType* iv : m_iverts)
				{
					points.push_back(iv);
				}

				for (uint i = 0; i<points.size(); i++)
				{
					points[i]->setIndex(i);
				}

				// split edges and marshall data
				// for safety, we zero out references to pre-subdivided edges,
				// which may have been destroyed

				VectorOfGenericEdgePointers						edges;

				for (uint k = 0; k<3; k++)
				{
					subdivideEdge(oedges[k], edges);
					oedges[k] = nullptr;
				}

				for (IsctEdgeType* ie : m_iedges)
				{
					subdivideEdge(ie, edges);
					ie = nullptr;
				}

				// find 2 dimensions to project onto get normal

				Cork::Math::Vector3D normal = cross( overts[1]->coordinate() - overts[0]->coordinate(),
													 overts[2]->coordinate() - overts[0]->coordinate() );
				uint normdim = maxDim( abs( normal ));
				uint dim0 = (normdim + 1) % 3;
				uint dim1 = (normdim + 2) % 3;
				double sign_flip = (normal[normdim] < 0.0) ? -1.0 : 1.0;

				struct triangulateio in, out;

				/* Define input points. */
				in.numberofpoints = (int)points.size();
				in.numberofpointattributes = 0;

				std::unique_ptr<REAL>		pointList( (REAL*)(malloc( sizeof(REAL) * in.numberofpoints * 2 )) );
				std::unique_ptr<int>		pointMarkerList( (int*)(malloc( sizeof(int) * in.numberofpoints )) );

				in.pointlist = pointList.get();
				in.pointmarkerlist = pointMarkerList.get();
				in.pointattributelist = nullptr;

				for (int k = 0; k<in.numberofpoints; k++)
				{
					in.pointlist[k * 2 + 0] = points[k]->coordinate()[dim0];
					in.pointlist[k * 2 + 1] = points[k]->coordinate()[dim1] * sign_flip;
					in.pointmarkerlist[k] = (points[k]->isBoundary()) ? 1 : 0;
				}

				/* Define the input segments */
				in.numberofsegments = (int)edges.size();
				in.numberofholes = 0;// yes, zero
				in.numberofregions = 0;// not using regions
				
				std::unique_ptr<int>		segmentList( (int*)(malloc( sizeof(int) * in.numberofsegments * 2 )) );
				std::unique_ptr<int>		segmentMarkerList( (int*)(malloc( sizeof(int) * in.numberofsegments )) );

				in.segmentlist = segmentList.get();
				in.segmentmarkerlist = segmentMarkerList.get();

				for (int k = 0; k<in.numberofsegments; k++)
				{
					in.segmentlist[k * 2 + 0] = edges[k]->ends()[0]->index();
					in.segmentlist[k * 2 + 1] = edges[k]->ends()[1]->index();

					in.segmentmarkerlist[k] = (edges[k]->boundary()) ? 1 : 0;
				}

				// to be safe... declare 0 triangle attributes on input
				in.numberoftriangles = 0;
				in.numberoftriangleattributes = 0;

				/* set for flags.... */
				out.pointlist = nullptr;
				out.pointattributelist = nullptr; // not necessary if using -N or 0 attr
				out.pointmarkerlist = nullptr;
				out.trianglelist = nullptr; // not necessary if using -E
				out.segmentlist = nullptr; // NEED THIS; output segments go here
				out.segmentmarkerlist = nullptr; // NEED THIS for OUTPUT SEGMENTS

				//	Solve the triangulation problem to get the orientation of the triangles correct.
				//
				//		The number of output points should always equal the number of input points.  I added the
				//		'j' option to cause triangle to jettison verticies not present in the final triangulation,
				//		which is usually the result of duplicated input vertices for our specific case in Cork.
				//		Alternatively, extra points may appear, I have found those can mostly be resolved by
				//		trying again with a different purturbation.

				char *params = (char*)("jpzQYY");

				triangulate(params, &in, &out, nullptr);


				std::unique_ptr<REAL>		outPointList( out.pointlist );
				std::unique_ptr<REAL>		outPointAttributeList( out.pointattributelist );
				std::unique_ptr<int>		outPointMarkerList( out.pointmarkerlist );
				std::unique_ptr<int>		outTriangleList( out.trianglelist );
				std::unique_ptr<int>		outSegmentList( out.segmentlist );
				std::unique_ptr<int>		outSegmentMarkerList( out.segmentmarkerlist );

				if (out.numberofpoints != in.numberofpoints)
				{
					//	When we end up here, it is usually because we have hit some self-intersections.

					return( SubdivideResult::Failure( SubdivideResultCodes::TRIANGULATE_OUT_POINT_COUNT_UNEQUAL_TO_IN_POINT_COUNT, "Unequal number of points before and after triangulation - check input meshes for self intersections." ) );
				}

				m_gtris.clear();

				for (int k = 0; k<out.numberoftriangles; k++)
				{
					GenericVertType*       gv0 = points[out.trianglelist[(k * 3) + 0]];
					GenericVertType*       gv1 = points[out.trianglelist[(k * 3) + 1]];
					GenericVertType*       gv2 = points[out.trianglelist[(k * 3) + 2]];

					m_gtris.push_back( m_iprob.newGenericTri(gv0, gv1, gv2));
				}

				return( SubdivideResult::Success() );
			}




		private:

			typedef	std::vector<GenericEdgeType*>			VectorOfGenericEdgePointers;


			IntersectionProblemBase&					m_iprob;
			const TopoTri&								m_triangle;

			IntersectionVertexPointerList				m_iverts;
			IntersectionEdgePointerList					m_iedges;

			//	Original triangle elements

			std::array<OrigVertType*, 3>				overts;
			std::array<OrigEdgeType*, 3>				oedges;

			GenericTriPointerList						m_gtris;



			// may actually not add edge, but instead just hook up endpoint

			void addEdge(IsctVertType* iv, const TopoTri& tri_key)
			{
				IsctEdgeType*		ie = nullptr;

				for (IsctEdgeType* currentIntersectionEdge : m_iedges)
				{
					if ( &(currentIntersectionEdge->otherTriKey().value()) == &tri_key)
					{
						ie = currentIntersectionEdge;
						break;
					}
				}


				if (ie)
				{ // if the edge is already present
					ie->ends()[1] = iv;
					iv->edges().push_back(ie);
				}
				else
				{ // if the edge is being added
					ie = m_iprob.newIsctEdge(iv, tri_key);
					m_iedges.push_back(ie);
				}
			}

			void addBoundaryHelper( const TopoEdge& edge, IsctVertType* iv)
			{
				iv->setBoundary(true);
				m_iverts.push_back(iv);

				// hook up point to boundary edge interior!

				for (uint k = 0; k<3; k++)
				{
					OrigEdgeType*   oe = oedges[k];

					if ( &(oe->concrete().value()) == &edge)
					{
						oe->interior().push_back(iv);
						iv->edges().push_back(oe);
						break;
					}
				}
			}

			void subdivideEdge(GenericEdgeType* ge, VectorOfGenericEdgePointers&		edges)
			{
				if (ge->interior().size() == 0)
				{
					edges.push_back(ge);
				}
				else if (ge->interior().size() == 1)
				{ // common case
					SplitEdgeType*       se0 = m_iprob.newSplitEdge(ge->ends()[0],
						ge->interior()[0],
						ge->boundary());

					SplitEdgeType*       se1 = m_iprob.newSplitEdge(ge->interior()[0],
						ge->ends()[1],
						ge->boundary());

					edges.push_back(se0);
					edges.push_back(se1);

					// get rid of old edge

					m_iprob.releaseEdge(ge);
				}
				else
				{
					// sorting is the uncommon case
					// determine the primary dimension and direction of the edge

					Cork::Math::Vector3D		dir = ge->ends()[1]->coordinate() - ge->ends()[0]->coordinate();
					uint						dim = (fabs(dir.x()) > fabs(dir.y())) ? ((fabs(dir.x()) > fabs(dir.z())) ? 0 : 2) :	((fabs(dir.y()) > fabs(dir.z())) ? 1 : 2);
					double						sign = (dir[dim] > 0.0) ? 1.0 : -1.0;

					// pack the interior vertices into a vector for sorting

					std::vector< std::pair<double, IsctVertType*> > verts;
					verts.reserve( ge->interior().size() );

					for (IsctVertType* iv : ge->interior())
					{
						// if the sort is ascending, then we're good...

						verts.emplace_back(std::make_pair(sign * iv->coordinate()[dim], iv));
					}

					// ... and sort the vector

					std::sort(verts.begin(), verts.end());

					// then, write the verts into a new container with the endpoints

					std::vector<GenericVertType*>  allv(verts.size() + 2);

					allv[0] = ge->ends()[0];
					allv[allv.size() - 1] = ge->ends()[1];

					for (uint k = 0; k<verts.size(); k++)
					{
						allv[k + 1] = verts[k].second;
					}

					// now create and accumulate new split edges

					for (uint i = 1; i < allv.size(); i++)
					{
						SplitEdgeType*   se = m_iprob.newSplitEdge( allv[i - 1], allv[i], ge->boundary() );
						edges.push_back( se );
					}

					// get rid of old edge

					m_iprob.releaseEdge( ge );
				}
			}

		};



		typedef tbb::concurrent_vector<TriangleProblem>		TriangleProblemList;



		class IntersectionProblemWorkspace : public IntersectionProblemWorkspaceBase
		{
		public:

			IntersectionProblemWorkspace()
			{
				m_gluePointMarkerPool.reserve(100000);
				m_isctVertexTypePool.reserve(200000);
				m_isctEdgeTypePool.reserve(300000);
				m_genericTriTypePool.reserve(100000);
				m_triangleProblemList.reserve(100000);
				m_isctVertexPointerPool.reserve(200000);
				m_isctEdgePointerPool.reserve(100000);
				m_genericTriPointerPool.reserve(100000);
			}

			virtual ~IntersectionProblemWorkspace() noexcept
			{};


			void		reset()
			{
				m_gluePointMarkerPool.clear();
				m_isctVertexTypePool.clear();
				m_isctEdgeTypePool.clear();
				m_genericTriTypePool.clear();
				m_triangleProblemList.clear();
				m_isctVertexPointerPool.clear();
				m_isctEdgePointerPool.clear();
				m_genericTriPointerPool.clear();

				m_AABVHWorkspace.reset();
			}


			operator GluePointMarkerList::PoolType&()
			{
				return(m_gluePointMarkerPool);
			}

			operator IsctVertTypeList::PoolType&()
			{
				return(m_isctVertexTypePool);
			}

			operator IsctEdgeTypeList::PoolType&()
			{
				return(m_isctEdgeTypePool);
			}

			operator GenericTriTypeList::PoolType&()
			{
				return(m_genericTriTypePool);
			}

			operator TriangleProblemList&()
			{
				return(m_triangleProblemList);
			}

			operator IntersectionVertexPointerList::PoolType&()
			{
				return(m_isctVertexPointerPool);
			}

			operator IntersectionEdgePointerList::PoolType&()
			{
				return(m_isctEdgePointerPool);
			}

			operator GenericTriPointerList::PoolType&()
			{
				return(m_genericTriPointerPool);
			}


			operator Cork::AABVH::Workspace&( )
			{
				return( m_AABVHWorkspace );
			}



		private:

			GluePointMarkerList::PoolType				m_gluePointMarkerPool;
			IsctVertTypeList::PoolType					m_isctVertexTypePool;		//	Covers both the intersection and original vertex types
			IsctEdgeTypeList::PoolType					m_isctEdgeTypePool;
			GenericTriTypeList::PoolType				m_genericTriTypePool;
			TriangleProblemList							m_triangleProblemList;

			IntersectionVertexPointerList::PoolType		m_isctVertexPointerPool;
			IntersectionEdgePointerList::PoolType		m_isctEdgePointerPool;
			GenericTriPointerList::PoolType				m_genericTriPointerPool;

			 Cork::AABVH::Workspace						m_AABVHWorkspace;
		};


		typedef SEFUtility::CachingFactory<Cork::Intersection::IntersectionProblemWorkspace>		IntersectionWorkspaceFactory;





		class IntersectionProblem : public IntersectionProblemBase, public IntersectionProblemIfx
		{
		public:

			IntersectionProblem( MeshBase&								owner,
								 const Quantization::Quantizer&			quantizer,
								 const Cork::Math::BBox3D&				intersectionBBox,
								 SEFUtility::CachingFactory<Cork::Intersection::IntersectionProblemWorkspace>::UniquePtr&		workspace );

    
			virtual ~IntersectionProblem()
			{
				reset();
			}
    

			//	Implementation of IntersectionProblemIfx
    
			IntersectionProblemResult			FindIntersections();
			IntersectionProblemResult			ResolveAllIntersections();

			SelfIntersectionStats				CheckSelfIntersection();			// test for self intersections in the mesh

			void								commit()
			{
				TopoCache::commit();
			}



			//	Other IntersectionProblem methods
    
			TriangleProblem*		getTprob(const TopoTri& t)
			{
				TriangleProblem*		prob = reinterpret_cast<TriangleProblem*>(const_cast<void*>(t.data()));

				if (!prob)
				{
					m_triangleProblemList.emplace_back(*this, t);
					prob = &(m_triangleProblemList.back());
				}

				return(prob);
			}


			void					dumpIsctEdges( std::vector< std::pair<Cork::Math::Vector3D, Cork::Math::Vector3D> >*			edges )
			{
				edges->clear();

				for (auto& tprob : m_triangleProblemList)
				{
					for (IsctEdgeType* ie : tprob.iedges())
					{
						GenericVertType* gv0 = ie->ends()[0];
						GenericVertType* gv1 = ie->ends()[1];

						edges->push_back( std::make_pair( gv0->coordinate(), gv1->coordinate() ) );
					}
				}
			}




		private:

			SEFUtility::CachingFactory<Cork::Intersection::IntersectionProblemWorkspace>::UniquePtr		m_workspace;

			TriangleProblemList&																		m_triangleProblemList;


			// if we encounter ambiguous degeneracies, then this
			// routine returns false, indicating that the computation aborted.


			enum TryToFindIntersectionsResultCodes {
				SUCCESS = 0,
				OUT_OF_MEMORY,
				TRI_EGDE_DEGENERACIES,
				TRI_TRI_TRI_INTERSECTIONS_FAILED
			};

			typedef SEFUtility::Result<TryToFindIntersectionsResultCodes>		TryToFindIntersectionsResult;


			TryToFindIntersectionsResult tryToFindIntersections();
			bool findTriTriTriIntersections();

			void reset();
	

			void createRealTriangles( TriangleProblem& tprob, EdgeCache &ecache )
			{
				for (auto& gt : tprob.gtris())
				{
					TopoTri* t = TopoCache::newTri();

					std::array<TopoVert*,3>		vertices;
					std::array<TopoEdge*,3>		edges;

					GenericTriType*		genericTri = gt.pointer();

					genericTri->setConcreteTriangle( t );

					CorkTriangle&		tri = TopoCache::ownerMesh().triangles()[t->ref()];

					for (uint k = 0; k<3; k++)
					{
						TopoVert&    v = genericTri->vertices()[k]->concreteVertex().value();
						vertices[k] = &v;
						tri[k] = v.ref();

						edges[k] = ecache.getTriangleEdge( genericTri, k, tprob.triangle() );
					}

					t->setVertices( vertices );
					t->setEdges( edges );

					fillOutTriData( *t, tprob.triangle() );
				}

				//	Once all the pieces are hooked up, let's kill the old triangle!
				//		We need to cast away the const here as well...

				TopoCache::deleteTri( &( const_cast<TopoTri&>( tprob.triangle() ) ) );
			}

		};







		IntersectionProblemBase::IntersectionProblemBase( MeshBase&								owner,
														  const Quantization::Quantizer&		quantizer,
														  const Cork::Math::BBox3D&				intersectionBBox,
														  IntersectionProblemWorkspaceBase&		workspace )
			: TopoCache( owner, workspace ),
			  m_quantizer(quantizer),
			  m_perturbation(quantizer),
			  m_intersectionBBox( make_aligned<Cork::Math::BBox3D>( intersectionBBox )),
			  m_workspace( workspace ),
			  m_gluePointMarkerList( workspace ),
			  m_isctVertTypeList( workspace ),
			  m_origVertTypeList( workspace ),
			  m_isctEdgeTypeList( workspace ),
			  m_origEdgeTypeList( workspace ),
			  m_splitEdgeTypeList( workspace ),
			  m_genericTriTypeList( workspace )
		{
			//	Initialize all the triangles to NOT have an associated tprob
			//		and set the boolAlgData value based on the input triangle

			for (TopoTri& t : triangles())
			{
				t.setData( nullptr );
				t.setBoolAlgData( ownerMesh().triangles()[t.ref()].boolAlgData() );
			}

			//	Initialize all of the edge solid IDs

			for (TopoEdge& e : edges())
			{
				e.setBoolAlgData( e.triangles().front()->boolAlgData() );
			}

			// and use vertex auxiliary data to store quantized vertex coordinates

			m_quantizedCoords.reserve( TopoCache::ownerMesh().vertices().size() );
	
			for (TopoVert& v : TopoCache::vertices())
			{
				m_quantizedCoords.emplace_back( m_quantizer.quantize( ownerMesh().vertices()[v.ref()] ) );

				v.setQuantizedValue( &(m_quantizedCoords.back() ) );
			}
		}



		IntersectionProblem::IntersectionProblem( MeshBase&								owner,
												  const Quantization::Quantizer&		quantizer,
												  const Cork::Math::BBox3D&				intersectionBBox,
												  SEFUtility::CachingFactory<Cork::Intersection::IntersectionProblemWorkspace>::UniquePtr&		workspace )
			: IntersectionProblemBase( owner, quantizer, intersectionBBox, *( workspace.get() ) ),
			  m_workspace( std::move( workspace )),
			  m_triangleProblemList( *(m_workspace.get()) )
		{}



		IntersectionProblem::TryToFindIntersectionsResult IntersectionProblem::tryToFindIntersections()
		{
			m_exactArithmeticContext.degeneracy_count = 0;

			TriangleAndIntersectingEdgesQueue			trianglesAndEdges;

			tbb::task_group		taskGroup;

			taskGroup.run( [&] { FindEdgeAndTriangleIntersections(Cork::AABVH::IntersectionType::BOOLEAN_INTERSECTION, trianglesAndEdges); } );

			while( true )
			{
				TriAndEdgeQueueMessage*			msg;

				trianglesAndEdges.pop( msg );

				const std::unique_ptr<TriAndEdgeQueueMessage> currentMessage( msg );

				if( currentMessage->type() == TriAndEdgeQueueMessage::MessageType::END_OF_MESSAGES )
				{
					break;
				}

				TopoTri&	triangle = (( TriangleAndIntersectingEdgesMessage&)(*currentMessage)).triangle();

				for ( const TopoEdge* edge : ( (TriangleAndIntersectingEdgesMessage&)( *currentMessage ) ).edges() )
				{
					if (triangle.intersectsEdge( *edge, m_quantizer, m_exactArithmeticContext))
					{
						GluePointMarker*      glue = m_gluePointMarkerList.emplace_back(false, true, *edge, triangle);

						// first add point and edges to the pierced triangle

						IsctVertType* iv = getTprob(triangle)->addInteriorEndpoint( *edge, *glue);

						for (auto tri : edge->triangles())
						{
							getTprob(*tri)->addBoundaryEndpoint(triangle, *edge, iv);
						}
					}

					if (m_exactArithmeticContext.degeneracy_count != 0)
					{
						break;
					}
				}

				if (m_exactArithmeticContext.degeneracy_count != 0)
				{
					break;
				}
			};

			taskGroup.wait();

			if (m_exactArithmeticContext.degeneracy_count > 0)
			{
				return(TryToFindIntersectionsResult::Failure(TryToFindIntersectionsResultCodes::TRI_EGDE_DEGENERACIES, "Degeneracies Detected during Triangle Edge instersection computations." ));
			}

			if (!findTriTriTriIntersections())
			{
				return(TryToFindIntersectionsResult::Failure(TryToFindIntersectionsResultCodes::TRI_TRI_TRI_INTERSECTIONS_FAILED, "Three Triangle Intersection computation failed."));
			}

			return(TryToFindIntersectionsResult::Success());
		}



		bool IntersectionProblem::findTriTriTriIntersections()
		{
			// we're going to peek into the triangle problems in order to
			// identify potential candidates for Tri-Tri-Tri intersections

			std::vector<TriTripleTemp> triples;
    
			for ( auto& tprob : m_triangleProblemList )
			{
				const TopoTri& t0 = tprob.triangle();
    
				// Scan pairs of existing edges to create candidate triples
        
				for (IntersectionEdgePointerList::iterator ie1 = tprob.iedges().begin(); ie1 != tprob.iedges().end(); ie1++)
				{
					IntersectionEdgePointerList::iterator itrNext = ie1;

					for (IntersectionEdgePointerList::iterator ie2 = ++itrNext; ie2 != tprob.iedges().end(); ie2++)
					{
						const TopoTri& t1 = ie1->pointer()->otherTriKey().value();
						const TopoTri& t2 = ie2->pointer()->otherTriKey().value();

						// This triple might be considered three times,
						// one for each triangle it contains.
						// To prevent duplication, only proceed if this is
						// the least triangle according to an arbitrary ordering

						if ((&t0 < &t1) && (&t0 < &t2))
						{
							// now look for the third edge.  We're not
							// sure if it exists...

							TriangleProblem*	prob1 = reinterpret_cast<TriangleProblem*>(t1.data());

							for (IsctEdgeType* ie : prob1->iedges())
							{
								if (&(ie->otherTriKey().value()) == &t2)
								{
									// ADD THE TRIPLE
									triples.emplace_back(TriTripleTemp(t0, t1, t2));
								}
							}
						}
					}
				}
			}

			// Now, we've collected a list of Tri-Tri-Tri intersection candidates.
			// Check to see if the intersections actually exist.
    
			for(TriTripleTemp& t : triples)
			{
				if ( !checkIsct( t.t0, t.t1, t.t2 ) )
				{
					continue;
				}

				// Abort if we encounter a degeneracy
        
				if (m_exactArithmeticContext.degeneracy_count > 0)
				{
					break;
				}

				GluePointMarker*      glue = m_gluePointMarkerList.emplace_back(true, false, t.t0, t.t1, t.t2);

				getTprob(t.t0)->addInteriorPoint( t.t1, t.t2, *glue);
				getTprob(t.t1)->addInteriorPoint( t.t0, t.t2, *glue);
				getTprob(t.t2)->addInteriorPoint( t.t0, t.t1, *glue);
			}
    
			if (m_exactArithmeticContext.degeneracy_count > 0)
			{
				return( false );   // restart / abort
			}
    
			return( true );
		}



		void IntersectionProblem::reset()
		{
			// the data pointer in the triangles points to tproblems
			// that we're about to destroy,
			// so zero out all those pointers first!
 
			for( auto& tprob : m_triangleProblemList )
			{
				tprob.ResetTopoTriLink();
			} 
    
			m_gluePointMarkerList.clear();
	
			m_isctVertTypeList.clear();
			m_origVertTypeList.clear();

			m_isctEdgeTypeList.clear();
			m_origEdgeTypeList.clear();
			m_splitEdgeTypeList.clear();

			m_genericTriTypeList.clear();

			m_triangleProblemList.clear();

			( dynamic_cast<IntersectionProblemWorkspace*>( m_workspace.get() ) )->reset();
		}


		IntersectionProblem::IntersectionProblemResult		IntersectionProblem::FindIntersections()
		{
			perturbPositions(); // always perturb for safety...

			boolean			foundIntersections = false;

			do
			{
				TryToFindIntersectionsResult		result = tryToFindIntersections();

				foundIntersections = result.Succeeded();

				if( !foundIntersections )
				{
					if (result.errorCode() == TryToFindIntersectionsResultCodes::OUT_OF_MEMORY)
					{
						return(IntersectionProblemResult::Failure(IntersectionProblemResultCodes::OUT_OF_MEMORY, "Out of Memory", result));
					}

					reset();

					auto	perturbAdjustResult = m_perturbation.adjust();

					if (!perturbAdjustResult.Succeeded())
					{
						return(IntersectionProblemResult::Failure(IntersectionProblemResultCodes::EXHAUSTED_PURTURBATION_RETRIES, "Perturbation adjustment failed", perturbAdjustResult));
					}

					perturbPositions();
				}
			}
			while( !foundIntersections );
    
			// ok all points put together,
			// all triangle problems assembled.
			// Some intersection edges may have original vertices as endpoints
			// we consolidate the problems to check for cases like these.

			for ( auto& tprob : m_triangleProblemList )
			{
				if( !tprob.Consolidate().Succeeded())
				{	
					return( IntersectionProblemResult::Failure( IntersectionProblemResultCodes::CONSOLIDATE_FAILED, "Consolidate failed" ) );
				}
			}

			return( IntersectionProblemResult::Success() );
		}



		SelfIntersectionStats			IntersectionProblem::CheckSelfIntersection()
		{
			Cork::Empty3d::ExactArithmeticContext		localArithmeticContext;
			unsigned int								numIntersections = 0;

			if( m_edgeBVH.get() == nullptr )
			{
				CreateBoundingVolumeHierarchy();
			}

			TopoEdgePointerVector			edges;

			for( TopoTri& t : triangles() )
			{
				m_edgeBVH->EdgesIntersectingTriangle( t, Cork::AABVH::IntersectionType::SELF_INTERSECTION, edges );

				for( const TopoEdge* edge : edges )
				{
					if( t.intersectsEdge( *edge, m_quantizer, localArithmeticContext ) )
					{
						numIntersections++;
					}
				}

				edges.clear();
			}

			return( SelfIntersectionStats( numIntersections ) );
		}


		IntersectionProblem::IntersectionProblemResult		IntersectionProblem::ResolveAllIntersections()
		{
			//	Subdivide the mesh in each triangle problem.

			for ( auto& tprob : m_triangleProblemList )
			{
				auto result = tprob.Subdivide();

				if( !result.Succeeded() )
				{
					//	Usually, we fail here as a result of a self-intersecting mesh.  Check for that condition now
					//		but we only have to check the current collection of triangles associated with this problem.
					//		We can do that directly using the bounding volume hierarchy.

					Cork::Empty3d::ExactArithmeticContext		localArithmeticContext;
					unsigned int								numIntersections = 0;

					std::set<const TopoTri*>					allTris;

					allTris.insert( &tprob.triangle() );

					for( auto edge : tprob.iedges() )
					{
						allTris.insert( &edge->otherTriKey().value() );
					}

					for( auto tri : allTris )
					{
						TopoEdgePointerVector			edges;

						m_edgeBVH->EdgesIntersectingTriangle( *tri, Cork::AABVH::IntersectionType::SELF_INTERSECTION, edges );
					
						for( const TopoEdge* edge : edges )
						{
							if( tri->intersectsEdge( *edge, m_quantizer, localArithmeticContext ) )
							{
								numIntersections++;
							}
						}

						edges.clear();
					}

					if( numIntersections > 0 )
					{
						return( IntersectionProblemResult::Failure( IntersectionProblemResultCodes::SELF_INTERSECTING_MESH, "Self Intersections found in Mesh", result ) );
					}
					
					return( IntersectionProblemResult::Failure( IntersectionProblemResultCodes::SUBDIVIDE_FAILED, "Subdivide failed", result ));
				}
			}
    
			// now we have diced up triangles inside each triangle problem
    
			// Let's go through the glue points and create a new concrete
			// vertex object for each of these.

			for (auto& glue : m_gluePointMarkerList)
			{
				createRealPtFromGluePt(glue);
			}
    
			EdgeCache ecache( *this );
    
			// Now that we have concrete vertices plugged in, we can
			// go through the diced triangle pieces and create concrete triangles
			// for each of those.
			// Along the way, let's go ahead and hook up edges as appropriate
    
			for ( auto& tprob : m_triangleProblemList )
			{
				createRealTriangles(tprob, ecache);
			}
   
			// mark all edges as normal by zero-ing out the data 

			for( auto& e : TopoCache::edges() )
			{
				e.setData( 0 );
			}

			// then iterate over the edges formed by intersections
			// (i.e. those edges without the boundary flag set in each triangle)
			// and mark those by setting the data pointer

			for (IsctEdgeType& ie : m_isctEdgeTypeList )
			{
				// every ie must be non-boundary
				TopoEdge* e = ecache.maybeEdge(&ie);
				e->setData( (void*)1 );
			}

			for (auto& se : m_splitEdgeTypeList)
			{
				TopoEdge* e = ecache.maybeEdge( &se );
				e->setData( (void*)1 );
			}
    
			// This basically takes care of everything EXCEPT one detail *) The base mesh data structures still need to be compacted
			// This detail should be handled by the calling code...

			return( IntersectionProblemResult::Success() );
		}





		std::unique_ptr<IntersectionProblemIfx>		IntersectionProblemIfx::GetProblem( MeshBase&							owner,
																						const Quantization::Quantizer&		quantizer,
																						const Cork::Math::BBox3D&			intersectionBBox )
		{
			IntersectionWorkspaceFactory::UniquePtr		workspace( IntersectionWorkspaceFactory::GetInstance() );

			return( std::unique_ptr<IntersectionProblemIfx>( new IntersectionProblem( owner, quantizer, intersectionBBox, workspace ) ));
		}


	}	//	namespace Intersection
}	//	namespace Cork



//
//	Create the IntersectionProblemWorkspace Factory down here as it has to follow declaration of the workspace and be in global scope
//

SEFUtility::CachingFactory<Cork::Intersection::IntersectionProblemWorkspace>::CacheType		SEFUtility::CachingFactory<Cork::Intersection::IntersectionProblemWorkspace>::m_cache;






