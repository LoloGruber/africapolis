import geopandas as gpd
import argparse
import momepy
from pathlib import Path

def analysis(settlements):
    settlements["eri"] =momepy.equivalent_rectangular_index(settlements)
    return settlements

def main():
    parser = argparse.ArgumentParser(description='Analyse settlement patterns in vector data')
    parser.add_argument('input_file', type=str, help='Path to the input vector file')
    args = parser.parse_args()
    settlements = gpd.read_file(args.input_file)
    settlements.to_crs(epsg=3857, inplace=True)  # Reproject to a metric CRS for accurate analysis
    result = analysis(settlements)
    result.to_crs(epsg=4326, inplace=True)  # Reproject back to WGS84 for output
    output_file = f'{Path(args.input_file).stem}_analysis.shp'
    result.to_file(output_file)

if __name__ == "__main__":
    main()
