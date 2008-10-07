/*
 * LEGAL NOTICE
 * This computer software was prepared by Battelle Memorial Institute,
 * hereinafter the Contractor, under Contract No. DE-AC05-76RL0 1830
 * with the Department of Energy (DOE). NEITHER THE GOVERNMENT NOR THE
 * CONTRACTOR MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR ASSUMES ANY
 * LIABILITY FOR THE USE OF THIS SOFTWARE. This notice including this
 * sentence must appear on any copies of this computer software.
 * 
 * EXPORT CONTROL
 * User agrees that the Software will not be shipped, transferred or
 * exported into any country or used in any manner prohibited by the
 * United States Export Administration Act or any other applicable
 * export laws, restrictions or regulations (collectively the "Export Laws").
 * Export of the Software may require some form of license or other
 * authority from the U.S. Government, and failure to obtain such
 * export control license may result in criminal liability under
 * U.S. laws. In addition, if the Software is identified as export controlled
 * items under the Export Laws, User represents and warrants that User
 * is not a citizen, or otherwise located within, an embargoed nation
 * (including without limitation Iran, Syria, Sudan, Cuba, and North Korea)
 *     and that User is not otherwise prohibited
 * under the Export Laws from receiving the Software.
 * 
 * All rights to use the Software are granted on condition that such
 * rights are forfeited if User fails to comply with the terms of
 * this Agreement.
 * 
 * User agrees to identify, defend and hold harmless BATTELLE,
 * its officers, agents and employees from all liability involving
 * the violation of such Export Laws, either directly or indirectly,
 * by User.
 */

/*! 
 * \file energy_input.cpp
 * \ingroup Objects
 * \brief The EnergyInput class source file.
 * \author Josh Lurz
 */

#include "util/base/include/definitions.h"
#include <xercesc/dom/DOMNode.hpp>
#include <xercesc/dom/DOMNodeList.hpp>
#include <xercesc/dom/DOMNamedNodeMap.hpp>
#include <cmath>

#include "functions/include/energy_input.h"
#include "containers/include/scenario.h"
#include "marketplace/include/marketplace.h"
#include "util/base/include/xml_helper.h"
#include "technologies/include/icapture_component.h"
#include "functions/include/icoefficient.h"
#include "functions/include/efficiency.h"
#include "functions/include/intensity.h"
#include "containers/include/dependency_finder.h"
#include "containers/include/iinfo.h"
#include "functions/include/function_utils.h"

using namespace std;
using namespace xercesc;

extern Scenario* scenario;

// static initialize.
const string EnergyInput::XML_REPORTING_NAME = "input-energy";

/*! \brief Get the XML node name in static form for comparison when parsing XML.
*
* This public function accesses the private constant string, XML_NAME. This way
* the tag is always consistent for both read-in and output and can be easily
* changed. The "==" operator that is used when parsing, required this second
* function to return static.
* \note A function cannot be static and virtual.
* \author Josh Lurz, James Blackwood
* \return The constant XML_NAME as a static.
*/
const string& EnergyInput::getXMLNameStatic() {
    const static string XML_NAME = "minicam-energy-input";
    return XML_NAME;
}

/*! \brief Get the XML name for reporting to XML file.
*
* This public function accesses the private constant string, XML_NAME. This way
* the tag is always consistent for reporting outputs and can be easily
* changed.
* \author Sonny Kim
* \return The constant XML_NAME.
*/
const string& EnergyInput::getXMLReportingName() const{
    return XML_REPORTING_NAME;
}

//! Constructor
EnergyInput::EnergyInput()
: mPhysicalDemand( scenario->getModeltime()->getmaxper() ),
  mCarbonContent( scenario->getModeltime()->getmaxper() ),
  mAdjustedCoefficients( scenario->getModeltime()->getmaxper() ),
  mPriceUnitConversionFactor( 1 )
{
}

/*!
 * \brief Destructor.
 * \note An explicit constructor must be defined to avoid the compiler inlining
 *       it in the header file before the header file for the type contained in
 *       the auto_ptr is included.
 */
EnergyInput::~EnergyInput() {
}

/*!
 * \brief Copy constructor.
 * \note This class requires a copy constructor because it has dynamically
 *          allocated memory.
 * \param aOther Energy input from which to copy.
 */
EnergyInput::EnergyInput( const EnergyInput& aOther ){
    // Do not clone the input coefficient as the calculated
    // coeffient will be filled out later.

    // Do not copy calibration values into the future
    // as they are only valid for one period.
    mName = aOther.mName;
    mIncomeElasticity = aOther.mIncomeElasticity;
    mTechChange = aOther.mTechChange;
    mPriceUnitConversionFactor = aOther.mPriceUnitConversionFactor;
    
    // Resize vectors to the correct size.
    mPhysicalDemand.resize( scenario->getModeltime()->getmaxper() );
    mCarbonContent.resize( scenario->getModeltime()->getmaxper() );
    mAdjustedCoefficients.resize( scenario->getModeltime()->getmaxper() );
}

EnergyInput* EnergyInput::clone() const {
    return new EnergyInput( *this );
}

bool EnergyInput::isSameType( const string& aType ) const {
    return aType == getXMLNameStatic();
}

void EnergyInput::XMLParse( const xercesc::DOMNode* node ) {
    // TODO: Replace this with the restructured XMLParse.
    // Make sure we were passed a valid node.
    assert( node );

    // get the name attribute.
    mName = XMLHelper<string>::getAttr( node, "name" );

    // get all child nodes.
    const DOMNodeList* nodeList = node->getChildNodes();

    // loop through the child nodes.
    for( unsigned int i = 0; i < nodeList->getLength(); i++ ){
        const DOMNode* curr = nodeList->item( i );
        if( curr->getNodeType() == DOMNode::TEXT_NODE ){
            continue;
        }

        const string nodeName = XMLHelper<string>::safeTranscode( curr->getNodeName() );
        if( nodeName == Efficiency::getXMLNameStatic() ) {
            mCoefficient.reset( new Efficiency( XMLHelper<double>::getValue( curr ) ) );
        }
        else if( nodeName == Intensity::getXMLNameStatic() ){
            mCoefficient.reset( new Intensity( XMLHelper<double>::getValue( curr ) ) );
        }
        else if( nodeName == "income-elasticity" ){
            mIncomeElasticity = XMLHelper<double>::getValue( curr );
        }
        else if( nodeName == "calibrated-value" ){
            mCalibrationInput = XMLHelper<double>::getValue( curr );
        }
        else if( nodeName == "tech-change" ){
            mTechChange = XMLHelper<double>::getValue( curr );
        }
        else if( nodeName == "price-unit-conversion" ){
            mPriceUnitConversionFactor = XMLHelper<double>::getValue( curr );
        }
        else if( nodeName == "keyword" ){
            DOMNamedNodeMap* keywordAttributes = curr->getAttributes();
            for( unsigned int attrNum = 0; attrNum < keywordAttributes->getLength(); ++attrNum ) {
                DOMNode* attrTemp = keywordAttributes->item( attrNum );
                mKeywordMap[ XMLHelper<string>::safeTranscode( attrTemp->getNodeName() ) ] = 
                    XMLHelper<string>::safeTranscode( attrTemp->getNodeValue() );
            }
        }
        else {
            ILogger& mainLog = ILogger::getLogger( "main_log" );
            mainLog.setLevel( ILogger::WARNING );
            mainLog << "Unrecognized text string: " << nodeName << " found while parsing "
                    << getXMLNameStatic() << "." << endl;
        }
    }
}

void EnergyInput::toInputXML( ostream& aOut,
                               Tabs* aTabs ) const
{
    XMLWriteOpeningTag( getXMLNameStatic(), aOut, aTabs, mName );
    // Write out the coefficient if there is one.
    if( mCoefficient.get() ){
        mCoefficient->toInputXML( aOut, aTabs );
    }
    XMLWriteElementCheckDefault( mIncomeElasticity, "income-elasticity", aOut,
                                 aTabs, Value( 0 ) );
    XMLWriteElementCheckDefault( mCalibrationInput, "calibrated-value", aOut,
                                 aTabs, Value( 0 ) );
    XMLWriteElementCheckDefault( mTechChange, "tech-change", aOut,
                                 aTabs, Value( 0 ) );
    XMLWriteElementCheckDefault( mPriceUnitConversionFactor, "price-unit-conversion", aOut,
                                 aTabs, Value( 1 ) );
    if( !mKeywordMap.empty() ) {
        XMLWriteElementWithAttributes( "", "keyword", aOut, aTabs, mKeywordMap );
    }

    XMLWriteClosingTag( getXMLNameStatic(), aOut, aTabs );
}

void EnergyInput::toDebugXML( const int aPeriod,
                               ostream& aOut,
                               Tabs* aTabs ) const
{
    XMLWriteOpeningTag ( getXMLNameStatic(), aOut, aTabs, mName );
    // Write out the coefficient if there is one.
    if( mCoefficient.get() ){
        mCoefficient->toDebugXML( aPeriod, aOut, aTabs );
    }

    XMLWriteElement( mIncomeElasticity, "income-elasticity", aOut, aTabs );
    XMLWriteElement( mCalibrationInput.isInited() ? mCalibrationInput.get() : -1,
                     "calibrated-value", aOut, aTabs );
    XMLWriteElement( mTechChange.isInited() ? mTechChange.get() : -1,
                     "tech-change", aOut, aTabs );
    XMLWriteElement( mCurrentCalibration.isInited() ? mCurrentCalibration.get() : -1,
                     "curr-calibrated-value", aOut, aTabs );
    XMLWriteElement( mAdjustedCoefficients[ aPeriod ], "current-coef", aOut, aTabs );
    XMLWriteElement( mCO2Coefficient.isInited() ? mCO2Coefficient.get() : -1,
                     "cached-co2-coef", aOut, aTabs );
    XMLWriteElement( mPhysicalDemand[ aPeriod ], "physical-demand", aOut, aTabs );
    XMLWriteElement( mCarbonContent[ aPeriod ], "carbon-content", aOut, aTabs );
    XMLWriteElement( mPriceUnitConversionFactor, "price-unit-conversion", aOut, aTabs );
    XMLWriteClosingTag( getXMLNameStatic(), aOut, aTabs );
}

void EnergyInput::completeInit( const string& aRegionName,
                                const string& aSectorName,
                                const string& aSubsectorName,
                                const string& aTechName,
                                DependencyFinder* aDependencyFinder,
                                const IInfo* aTechInfo )
{

    // Add the input dependency to the dependency finder.
    aDependencyFinder->addDependency( aSectorName, mName );
    
    // Initialize the current calibration value.
    if( mCalibrationInput.isInited() ){
        mCurrentCalibration = mCalibrationInput;
    }

    // If there is a coefficient, initialize it and determine the current
    // coefficient. Otherwise use a default intensity of 1.
    double currCoef = 1;
    if( mCoefficient.get() ){
        mCoefficient->completeInit();
        currCoef = mCoefficient->getCoefficient();
    }
    // TODO: This needs a default here.

    // Set the coeffients to the read-in value.
    mAdjustedCoefficients.assign( mAdjustedCoefficients.size(),
                                  currCoef );
}

void EnergyInput::initCalc( const string& aRegionName,
                            const string& aSectorName,
                            const bool aIsNewInvestmentPeriod,
                            const bool aIsTrade,
                            const int aPeriod )
{
    // There must be a valid region name.
    assert( !aRegionName.empty() );

    // Initialize the coefficient from the marketplace.
    mCO2Coefficient = FunctionUtils::getCO2Coef( aRegionName, mName, aPeriod );

    // Set the coefficient for the current period if there is an explicit
    // coefficient read-in, or it was not initialized from the previous period.
    if( mCoefficient.get() ){
        mAdjustedCoefficients[ aPeriod ] = mCoefficient->getCoefficient();
    }
    else if( !mAdjustedCoefficients[ aPeriod ].isInited() ){
        mAdjustedCoefficients[ aPeriod ] = 1;
    }
}

void EnergyInput::copyParam( const IInput* aInput,
                             const int aPeriod )
{
    aInput->copyParamsInto( *this, aPeriod );
}

void EnergyInput::copyParamsInto( EnergyInput& aInput,
                                  const int aPeriod ) const
{
    assert( aPeriod > 0 );
    // If the current input did not explicitly read in a coefficient, copy
    // forward the coefficient from the previous period. This results in any
    // technical change from the previous periods being applied.
    // TODO: This has some strange consequences.
    if( !aInput.mCoefficient.get() ){
        aInput.mAdjustedCoefficients[ aPeriod ] =
            mAdjustedCoefficients[ aPeriod - 1 ];
    }
}

double EnergyInput::getCO2EmissionsCoefficient( const string& aGHGName,
                                             const int aPeriod ) const
{
    // Check that the CO2 coefficient is initialized.
    assert( mCO2Coefficient.isInited() );
    return mCO2Coefficient;
}

double EnergyInput::getPhysicalDemand( const int aPeriod ) const {
    assert( mPhysicalDemand[ aPeriod ].isInited() );
    return mPhysicalDemand[ aPeriod ];
}

double EnergyInput::getCarbonContent( const int aPeriod ) const {
    return mCarbonContent[ aPeriod ];
}

void EnergyInput::setPhysicalDemand( double aPhysicalDemand,
                                     const string& aRegionName,
                                     const int aPeriod )
{
    mPhysicalDemand[ aPeriod ].set( aPhysicalDemand );
    scenario->getMarketplace()->addToDemand( mName, aRegionName,
                                             mPhysicalDemand[ aPeriod ],
                                             aPeriod, true );
    mCarbonContent[ aPeriod ].set( aPhysicalDemand * mCO2Coefficient );
}

double EnergyInput::getCoefficient( const int aPeriod ) const {
    // Check that the coefficient has been initialized.
    assert( mAdjustedCoefficients[ aPeriod ].isInited() );

    return mAdjustedCoefficients[ aPeriod ];
}

void EnergyInput::setCoefficient( const double aCoefficient,
                                  const int aPeriod )
{
    // Coefficients must be positive. */
    assert( aCoefficient >= 0 );

    // Store the adjusted coefficient locally.
    mAdjustedCoefficients[ aPeriod ] = aCoefficient;
}

double EnergyInput::getPrice( const string& aRegionName,
                              const int aPeriod ) const
{
    return mPriceUnitConversionFactor *
        scenario->getMarketplace()->getPrice( mName, aRegionName, aPeriod );
}

void EnergyInput::setPrice( const string& aRegionName,
                            const double aPrice,
                            const int aPeriod )
{
    // Not hooking this up yet, it could work.
}

void EnergyInput::tabulateFixedQuantity( const string& aRegionName,
                                         const double aFixedOutput,
                                         const bool aIsInvestmentPeriod,
                                         const int aPeriod )
{
    // Get the existing calibrated demand from the marketplace.
    IInfo* marketInfo = scenario->getMarketplace()->getMarketInfo( mName, aRegionName,
                                                                   aPeriod, false );

    // Normal inputs must have markets. However an error in the input file may
    // cause them not to.
    if( !marketInfo ){
        // Could log an error here, however this error may have already been
        // printed and this would be a lot of messages.
        return;
    }

    const string CAL_DEMAND = "calDemand";
    double existingDemand = marketInfo->getDouble( CAL_DEMAND, false );
    
    // Check if it is already not fixed.
    const double DEMAND_VARIABLE = -1;

    // Calibrated output is preferred to calibrated input. Coefficients should
    // have already been adjusted so that these are equivalent.
    if( aFixedOutput != -1 ){
        // set demand for fuel in marketInfo counter
        // TODO: Remove leontief assumption.
        marketInfo->setDouble( CAL_DEMAND, aFixedOutput *
                               getCoefficient( aPeriod ) + max( existingDemand, 0.0 ) );

        // Ensure that if a calibration input exists it is equal to the
        // coefficient multiplied by the calibrated output.
        assert( !aIsInvestmentPeriod
                || !mCurrentCalibration.isInited()
                || util::isEqual( aFixedOutput *
                                  getCoefficient( aPeriod ), mCurrentCalibration.get() ) );
    }
    // Add the calibrated output to the fixed demand in the initial investment
    // period. A technology and its' inputs may operate for multiple periods
    // after the initial period. TODO: Is this right? If its not new investment
    // should it set demand to variable? Why doesn't that hit the fixed output
    // block?
    else if( aIsInvestmentPeriod && mCurrentCalibration.isInited() ){
        marketInfo->setDouble( CAL_DEMAND, mCurrentCalibration + max( existingDemand, 0.0 ) );
    }
    else {
        // If not fixed, then set to DEMAND_VARIABLE to indicate a demand that is not
        // completely fixed
        marketInfo->setDouble( CAL_DEMAND, DEMAND_VARIABLE );
    }
}

void EnergyInput::scaleCalibrationQuantity( const double aScaleFactor ){
    // Scale factor must be positive.
    assert( aScaleFactor >= 0 );

    // If the calibration value was read in than the current value should be
    // initialized. 
    assert( !mCalibrationInput.isInited() || mCurrentCalibration.isInited() );

    // Only scale the calibration input if it was initialized.
    if( mCurrentCalibration.isInited() ){
        mCurrentCalibration = mCurrentCalibration * aScaleFactor;
    }
}

double EnergyInput::getCalibrationQuantity( const int aPeriod ) const
{
    // If the calibration value was read in than the current value
    // should be initialized. 
    assert( !mCalibrationInput.isInited() || mCurrentCalibration.isInited() );
    
    return mCurrentCalibration.isInited() ? mCurrentCalibration.get() : -1;
}

bool EnergyInput::hasTypeFlag( const int aTypeFlag ) const {
    return ( ( aTypeFlag & ~IInput::ENERGY ) == 0 );
}

double EnergyInput::getIncomeElasticity() const {
    return mIncomeElasticity;
}

double EnergyInput::getPriceElasticity() const {
    return 0;
}

double EnergyInput::getTechChange( const int aPeriod ) const
{
    return mTechChange;
}

