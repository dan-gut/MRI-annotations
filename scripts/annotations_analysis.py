import numpy as np
import pandas as pd
import altair as alt
import matplotlib as mpl
import matplotlib.pyplot as plt
import os

# ANN_DIR = '/home/daniel/Pulpit/!FinalAnnotationsTask/annotations'
ANN_DIR = '/home/daniel/Pulpit/!FinalAnnotationsTask/annotations_postprocessed'

RATERS = ['WadimWojciechowski', 'IwonaKucybala', 'KamilKrupa', 'MiloszRozynek']

SPA_CASE_LIST = [4, 8, 9, 10, 11, 27, 33, 38, 39]
KNEE_CASE_LIST = [1, 5, 7, 10, 11, 12]

ANN_TYPES = ['KNEE1250LSC', 'KNEE1250TPS', 'KNEE2500LSC', "KNEE2500TPS", 'KNEEMANUAL',
             'SPA1000LSC', 'SPA1000TPS', 'SPA2000LSC', 'SPA2000TPS', 'SPAMANUAL']

GENERAL_ANN_NAMES = {'KNEE1250LSC': "LSC-lower", 'KNEE1250TPS': "TPS-lower",
                     'KNEE2500LSC': "LSC-higher", "KNEE2500TPS": "TPS-higher",
                     'SPA1000LSC': "LSC-lower", 'SPA1000TPS': "TPS-lower",
                     'SPA2000LSC': "LSC-higher", 'SPA2000TPS': "TPS-higher"}

SP_METHODS = ['LSC', 'TPS']
LOWER_VALUES = ['1000', '1250']
HIGHER_VALUES = ['2000', '2500']


def read_binary_data(filepath, img_size, slices_no, signed=False):
    if signed:
        img = np.zeros((img_size, img_size, slices_no), dtype=np.int8)
    else:
        img = np.zeros((img_size, img_size, slices_no), dtype=np.uint8)

    f = open(filepath, "rb")
    for i in range(slices_no):
        for j in range(img_size):
            for k in range(img_size):
                byte = f.read(1)
                val = byte[0]
                img[j, k, i] = val
    f.close()
    return img


def load_data():
    manual_annotations = {}
    combined_annotations = {}
    sp_annotations = {}
    
    for ann_type in ANN_TYPES:
        manual_annotations[ann_type] = {case_no: {} for case_no in SPA_CASE_LIST} if ann_type[:3] == "SPA" \
            else {case_no: {} for case_no in KNEE_CASE_LIST}
        combined_annotations[ann_type] = {case_no: {} for case_no in SPA_CASE_LIST} if ann_type[:3] == "SPA" \
            else {case_no: {} for case_no in KNEE_CASE_LIST}
        if ann_type[-6:] != "MANUAL":
            sp_annotations[ann_type] = {case_no: {} for case_no in SPA_CASE_LIST} if ann_type[:3] == "SPA" \
                else {case_no: {} for case_no in KNEE_CASE_LIST}

    for rater in os.listdir(ANN_DIR):
        if rater in RATERS:
            for ann_type in (os.listdir(f'{ANN_DIR}/{rater}/sp')):
                if ann_type in ANN_TYPES:
                    for case_filename in os.listdir(f'{ANN_DIR}/{rater}/sp/{ann_type}'):
                        if case_filename.endswith(".raw"):
                            _, case_no, img_size, _, slices_no, _, _ = case_filename.split('_')
                            img_size = int(img_size)
                            slices_no = int(slices_no)
                            case_no = int(case_no)
                            if (ann_type[:3] == 'SPA' and case_no not in SPA_CASE_LIST) or \
                                    (ann_type[:4] == 'KNEE' and case_no not in KNEE_CASE_LIST):
                                continue
                            sp_annotations[ann_type][case_no][rater] = \
                                read_binary_data(f'{ANN_DIR}/{rater}/sp/{ann_type}/{case_filename}',
                                                 img_size, slices_no, signed=False)

            for ann_type in (os.listdir(f'{ANN_DIR}/{rater}/manual')):
                if ann_type in ANN_TYPES:
                    for case_filename in os.listdir(f'{ANN_DIR}/{rater}/manual/{ann_type}'):
                        if case_filename.endswith(".raw"):
                            _, case_no, img_size, _, slices_no, _, _ = case_filename.split('_')
                            img_size = int(img_size)
                            slices_no = int(slices_no)
                            case_no = int(case_no)
                            if (ann_type[:3] == 'SPA' and case_no not in SPA_CASE_LIST) or \
                                    (ann_type[:4] == 'KNEE' and case_no not in KNEE_CASE_LIST):
                                continue
                            manual_annotations[ann_type][case_no][rater] = \
                                read_binary_data(f'{ANN_DIR}/{rater}/manual/{ann_type}/{case_filename}',
                                                 img_size, slices_no, signed=True)

                            if ann_type[-6:] != "MANUAL":
                                combined_annotations[ann_type][case_no][rater] = \
                                    sp_annotations[ann_type][case_no][rater] + \
                                    manual_annotations[ann_type][case_no][rater]

                                assert np.max(sp_annotations[ann_type][case_no][rater]) in (0, 1), \
                                    f'SP max value incorrect for {ann_type}, {rater}'
                                assert np.min(sp_annotations[ann_type][case_no][rater]) in (0, 1), \
                                    f'SP min value incorrect for {ann_type}, {rater}'

                                assert np.max(manual_annotations[ann_type][case_no][rater]) in (-1, 0, 1), \
                                    f'Manual max value incorrect for {ann_type}, {rater}'
                                assert np.min(manual_annotations[ann_type][case_no][rater]) in (-1, 0, 1), \
                                    f'Manual min value incorrect for {ann_type}, {rater}'

                                assert np.max(combined_annotations[ann_type][case_no][rater]) in (0, 1), \
                                    f'Combined max value incorrect for {ann_type}, {rater}'
                                assert np.max(combined_annotations[ann_type][case_no][rater]) in (0, 1), \
                                    f'Combined min value incorrect for {ann_type}, {rater}'
                            else:
                                combined_annotations[ann_type][case_no][rater] = \
                                    manual_annotations[ann_type][case_no][rater]

                                assert np.max(manual_annotations[ann_type][case_no][rater]) in (0, 1), \
                                    f'Manual max value incorrect for {ann_type}, {rater}'
                                assert np.min(manual_annotations[ann_type][case_no][rater]) in (0, 1), \
                                    f'Manual min value incorrect for {ann_type}, {rater}'

    return manual_annotations, sp_annotations, combined_annotations


def manual_to_sp_ratio(manual_ann, sp_ann):
    if np.sum(sp_ann) == 0:
        return -1
    else:
        return np.sum(np.abs(manual_ann))/np.sum(sp_ann)


def agreement_area_ratio(ann_list):
    ann_sum = np.sum(ann_list, axis=0)

    vals, counts = np.unique(ann_sum, return_counts=True)
    total_ann = np.count_nonzero(ann_sum)

    if total_ann == 0:
        return 0  # nothing is annotated

    ratios = {1: 0., 2: 0., 3: 0.}

    for val, count in zip(vals, counts):
        if val == 0:
            continue
        ratios[val] = count/total_ann

    return ratios


def plot_agreement_ratios(agreement_area_ratios, description=''):
    agr_area_1 = {}
    agr_area_2 = {}
    agr_area_3 = {}

    for ann_t, case_dict in agreement_area_ratios.items():
        if ann_t not in agr_area_1:
            agr_area_1[ann_t] = {}
        if ann_t not in agr_area_2:
            agr_area_2[ann_t] = {}
        if ann_t not in agr_area_3:
            agr_area_3[ann_t] = {}

        for case_n, ratios_dict in case_dict.items():
            agr_area_1[ann_t][case_n] = agreement_area_ratios[ann_t][case_n][1]
            agr_area_2[ann_t][case_n] = agreement_area_ratios[ann_t][case_n][2]
            agr_area_3[ann_t][case_n] = agreement_area_ratios[ann_t][case_n][3]

    def prep_df(df, name):
        df = df.stack().reset_index()
        df.columns = ['case', 'method', 'ratios']
        df['Agreement area'] = name
        return df

    chart_data_1 = prep_df(pd.DataFrame(agr_area_1), '1 rater')
    chart_data_2 = prep_df(pd.DataFrame(agr_area_2), '2 raters')
    chart_data_3 = prep_df(pd.DataFrame(agr_area_3), '3 raters')
    chart_data = pd.concat([chart_data_1, chart_data_2, chart_data_3])

    chart = alt.Chart(chart_data).mark_bar().encode(
        x=alt.X('method:N', title=None),  # field to group columns on
        y=alt.Y('sum(ratios):Q', axis=alt.Axis(grid=True, title=None)),  # field to use as Y values and how to calculate
        column=alt.Column('case:N', title=None),  # field to use as the set of columns to be  represented in each group
        color=alt.Color('Agreement area:N',  # field to use for color segmentation
                        scale=alt.Scale(range=['#2ca02c', '#ff7f0e', '#d62728'], ),  # color pallet
                        )) \
        .configure_view(strokeOpacity=0)  # remove grid lines around column clusters

    # import altair_viewer
    # altair_viewer.show(chart)
    chart.save(f'plots/agreement_area_{description}.png')


def prepare_ratio_data(ratio_dict_list, name_list):
    def transform_dict(ratio_dict):
        processed_ratio_dict = {ann_type: [] for ann_type in set(GENERAL_ANN_NAMES.values())}
        processed_ratio_stds_dict = {ann_type: [] for ann_type in set(GENERAL_ANN_NAMES.values())}

        for ann_type, ratios in ratio_dict.items():
            key = ''
            for method in SP_METHODS:
                if method in ann_type:
                    key += method + '-'
                    break
            if any(px_no in ann_type for px_no in LOWER_VALUES):
                key += 'lower'
            elif any(px_no in ann_type for px_no in HIGHER_VALUES):
                key += 'higher'
            else:
                raise ValueError('Incorrect superpixels number.')

            for ratio_list in ratios.values():
                processed_ratio_dict[key].extend(ratio_list)
                processed_ratio_stds_dict[key].extend(ratio_list)

        for ann_type, ratio_list in processed_ratio_dict.items():
            processed_ratio_dict[ann_type] = np.mean(ratio_list)
            processed_ratio_stds_dict[ann_type] = np.std(ratio_list)

        return processed_ratio_dict, processed_ratio_stds_dict

    mean_ratios_dict = {}
    std_ratios_dict = {}
    for ratio_dict, name in zip(ratio_dict_list, name_list):
        mean_dict, std_dict = transform_dict(ratio_dict)
        mean_ratios_dict[name] = mean_dict
        std_ratios_dict[name] = std_dict

    return mean_ratios_dict, std_ratios_dict


def plot_correction_ratio(manual_to_sp_ratios_dict, manual_to_sp_ratios_std_dict):
    df = pd.DataFrame(manual_to_sp_ratios_dict)
    err_bars = [[std_val for std_val in std_dict.values()] for std_dict in manual_to_sp_ratios_std_dict.values()]
    ax = df.plot(kind='bar', rot=0, yerr=err_bars, capsize=3)

    ax.set_axisbelow(True)
    ax.get_yaxis().set_minor_locator(mpl.ticker.AutoMinorLocator())
    ax.grid(which='major', axis='y', linestyle='solid', linewidth=0.6)
    ax.grid(which='minor', axis='y', linestyle='solid', linewidth=0.3)

    ax.set_ylabel("Manual corrections area to superpixel area ratio [-]")
    plt.subplots_adjust(left=0.12, right=0.97, top=0.97, bottom=0.08)

    plt.savefig(f'plots/manual_to_sp_ratio.pdf')
    plt.clf()


if __name__ == '__main__':
    manual_annotations, sp_annotations, combined_annotations = load_data()

    manual_to_sp_ratios_SPA = {}
    agr_area_ratios_SPA = {}
    manual_to_sp_ratios_KNEE = {}
    agr_area_ratios_KNEE = {}

    for ann_type, cases in manual_annotations.items():
        if ann_type[-6:] != "MANUAL":
            if ann_type[:3] == 'SPA' and ann_type not in manual_to_sp_ratios_SPA:
                manual_to_sp_ratios_SPA[ann_type] = {}
            elif ann_type[:4] == 'KNEE' and ann_type not in manual_to_sp_ratios_KNEE:
                manual_to_sp_ratios_KNEE[ann_type] = {}
        if ann_type[:3] == 'SPA' and ann_type not in agr_area_ratios_SPA:
            agr_area_ratios_SPA[ann_type] = {}
        elif ann_type[:4] == 'KNEE' and ann_type not in agr_area_ratios_KNEE:
            agr_area_ratios_KNEE[ann_type] = {}

        for case_no, raters in cases.items():
            if ann_type[-6:] != "MANUAL":
                m2s_ratios = []

                if ann_type[:3] == 'SPA':
                    for rater in raters.keys():
                        ratio = manual_to_sp_ratio(manual_annotations[ann_type][case_no][rater],
                                                   sp_annotations[ann_type][case_no][rater])
                        if ratio != -1:
                            m2s_ratios.append(ratio)

                    manual_to_sp_ratios_SPA[ann_type][case_no] = m2s_ratios

                elif ann_type[:4] == 'KNEE':
                    for rater in raters.keys():
                        ratio = manual_to_sp_ratio(manual_annotations[ann_type][case_no][rater],
                                                   sp_annotations[ann_type][case_no][rater])
                        if ratio != -1:
                            m2s_ratios.append(ratio)

                    manual_to_sp_ratios_KNEE[ann_type][case_no] = m2s_ratios

            if ann_type[:3] == 'SPA':
                agr_area_ratios_SPA[ann_type][case_no] = \
                    agreement_area_ratio(list(combined_annotations[ann_type][case_no].values()))
            elif ann_type[:4] == 'KNEE':
                agr_area_ratios_KNEE[ann_type][case_no] = \
                    agreement_area_ratio(list(combined_annotations[ann_type][case_no].values()))

    plot_agreement_ratios(agr_area_ratios_SPA, description='SPA')
    plot_agreement_ratios(agr_area_ratios_KNEE, description='KNEE')

    manual_to_sp_ratios_combined, manual_to_sp_ratios_std_combined = \
        prepare_ratio_data([manual_to_sp_ratios_SPA, manual_to_sp_ratios_KNEE], ['SIJ', 'Knee'])
    plot_correction_ratio(manual_to_sp_ratios_combined, manual_to_sp_ratios_std_combined)
